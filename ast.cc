#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Transforms/Scalar.h>
#include <iostream>

#include "ast.h"

using namespace std;
using namespace llvm;

// Error handling
static Value *ValueError(const char *error) {
  cerr << error << endl;
  return NULL;
}
static Function *FunctionError(const char *error) {
  cerr << error << endl;
  return NULL;
}

Executor::Executor() : TheContext(getGlobalContext()), Builder(TheContext) {
  InitializeNativeTarget();
  TheModule = new Module("parser module", TheContext);
  // Create the JIT execution engine
  TheEE = EngineBuilder(TheModule).create();
  // setup a function level optimizer
  TheFPM = new FunctionPassManager(TheModule);
  TheFPM->add(new DataLayout(*TheEE->getDataLayout()));
  TheFPM->add(createBasicAliasAnalysisPass());
  TheFPM->add(createInstructionCombiningPass());
  TheFPM->add(createReassociatePass());
  TheFPM->add(createGVNPass());
  TheFPM->add(createCFGSimplificationPass());
  TheFPM->doInitialization();
}

Executor::fptr Executor::Exec(Lexer &lexer) {
  // JIT the function returning a func ptr
  if (Function *F = ParseNext(lexer, *this)) {
    //F->dump();
    return (double(*)())TheEE->getPointerToFunction(F);
  }
  return NULL;
}

// ----------------------------------------------------------------------
NumberExprAST::NumberExprAST(double val) : Val(val) {}

Value *NumberExprAST::Codegen(Executor &ctx) {
  return ConstantFP::get(ctx.TheContext, APFloat(Val));
}

// ----------------------------------------------------------------------
VariableExprAST::VariableExprAST(const string &name) : Name(name) {}

Value *VariableExprAST::Codegen(Executor &ctx) {
  Value *v = ctx.NamedValues[Name];
  return v ? v : ValueError("Unknown variable name");
}

// ----------------------------------------------------------------------
UnaryExprAST::UnaryExprAST(Token::lexic_component op, ExprAST *expr)
    : Op(op), Expr(expr) {}

Value *UnaryExprAST::Codegen(Executor &ctx) {
  Value *V = Expr->Codegen(ctx);
  if (V == NULL)
    return NULL;
  if (Op != Token::tokMinus)
    return ValueError("Unknown unary operator");
  return ctx.Builder.CreateFNeg(V, "negtmp");
}

// ----------------------------------------------------------------------
BinaryExprAST::BinaryExprAST(Token::lexic_component op, ExprAST *lhs,
                             ExprAST *rhs)
    : Op(op), LHS(lhs), RHS(rhs) {}

Value *BinaryExprAST::Codegen(Executor &ctx) {
  Value *L = LHS->Codegen(ctx);
  Value *R = RHS->Codegen(ctx);
  if (L == NULL || R == NULL)
    return NULL;

  switch (Op) {
  case Token::tokLT:
    L = ctx.Builder.CreateFCmpULT(L, R, "cmptmp");
    // convert bool to double
    return ctx.Builder
        .CreateUIToFP(L, Type::getDoubleTy(ctx.TheContext), "booltmp");
  case Token::tokPlus:
    return ctx.Builder.CreateFAdd(L, R, "addtmp");
  case Token::tokMinus:
    return ctx.Builder.CreateFSub(L, R, "subtmp");
  case Token::tokMultiply:
    return ctx.Builder.CreateFMul(L, R, "multmp");
  case Token::tokDivide:
    return ctx.Builder.CreateFDiv(L, R, "divtmp");
  default:
    return ValueError("Invalid binary operator");
  }
}

// ----------------------------------------------------------------------
CallExprAST::CallExprAST(const string &callee, vector<ExprAST *> &args)
    : Callee(callee), Args(args) {}

Value *CallExprAST::Codegen(Executor &ctx) {
  // lookup our function in the global module table
  Function *CalleeF = ctx.TheModule->getFunction(Callee);
  if (CalleeF == NULL)
    return ValueError("Unknown function referenced");
  if (CalleeF->arg_size() != Args.size())
    return ValueError("Incorrect # of arguments");

  vector<Value *> ArgsV;
  //for_each(Args.begin(), Args.end(), [](Value *arg) { ArgsV.push_back(arg);
  //});
  for (unsigned i = 0; i < Args.size(); i++) {
    ArgsV.push_back(Args[i]->Codegen(ctx));
    if (ArgsV.back() == NULL)
      return NULL;
  }
  return ctx.Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

// ----------------------------------------------------------------------
PrototypeAST::PrototypeAST(const string &name, const vector<string> &args)
    : Name(name), Args(args) {}

// http://llvm.org/releases/3.3/docs/tutorial/LangImpl3.html#id4
Function *PrototypeAST::Codegen(Executor &ctx) {
  Function *F = NULL;
  if ((F = ctx.TheModule->getFunction(Name)) == NULL) {
    // make the function type: double(double, double) ... etc.
    vector<Type *> DblArgs(Args.size(), Type::getDoubleTy(ctx.TheContext));
    FunctionType *FT = // returns a double, takes n-doubles, is not vararg
        FunctionType::get(Type::getDoubleTy(ctx.TheContext), DblArgs, false);
    // register our function in TheModule with name Name
    F = Function::Create(FT, Function::ExternalLinkage, Name, ctx.TheModule);
  }

  if (!F->empty()) // check the function es a forward decl if it exists
    return FunctionError("Function cannot be redefined");
  if (F->arg_size() != Args.size()) // defined and empty => extern, check # args
    return FunctionError("Redefinition of function with wrong # of args");

  // set names for all arguments
  unsigned idx = 0;
  for (Function::arg_iterator AI = F->arg_begin(); idx != Args.size();
       ++AI, ++idx) {
    AI->setName(Args[idx]);
    // add arguments to variable sym-table
    ctx.NamedValues[Args[idx]] = AI;
  }
  return F;
}

// ----------------------------------------------------------------------
FunctionAST::FunctionAST(PrototypeAST *proto, ExprAST *body)
    : Proto(proto), Body(body) {}

Function *FunctionAST::Codegen(Executor &ctx) {
  ctx.NamedValues.clear(); // only let some variables be in scope
  Function *F = Proto->Codegen(ctx);
  if (F == NULL)
    return NULL;

  // Create a new basic block to start insertion into.
  BasicBlock *BB = BasicBlock::Create(ctx.TheContext, "entry", F);
  ctx.Builder.SetInsertPoint(BB);

  if (Value *RetVal = Body->Codegen(ctx)) {
    // finish off the function
    ctx.Builder.CreateRet(RetVal);
    //Validate the generated code, checking for consistency
    verifyFunction(*F);

    // Optimize the function of the Optimizer is available
    if (ctx.TheFPM != NULL)
      ctx.TheFPM->run(*F);

    return F;
  }
  // Error reading body, remove function from fsym-tab to let usr redefine it
  F->eraseFromParent();
  return NULL;
}

// ----------------------------------------------------------------------
IfExprAST::IfExprAST(ExprAST *cond, ExprAST *then, ExprAST *_else)
    : Cond(cond), Then(then), Else(_else) {}

Value *IfExprAST::Codegen(Executor &ctx) {
  Value *CondV = Cond->Codegen(ctx);
  if (CondV == NULL)
    return NULL;

  // convert condition to a bool by comparing equal to 0.0
  CondV = ctx.Builder.CreateFCmpONE(
      CondV, ConstantFP::get(ctx.TheContext, APFloat(0.0)), "ifcond");

  // ask the builder for the current basic block,
  // the parent of this BB is the function holding it
  Function *F = ctx.Builder.GetInsertBlock()->getParent();

  // create BBlocks for if/then/else cases
  // insert the "then" block at the end of the F function
  BasicBlock *ThenBB = BasicBlock::Create(ctx.TheContext, "then", F);
  BasicBlock *ElseBB = BasicBlock::Create(ctx.TheContext, "else");
  // where to resume after conditional
  BasicBlock *MergeBB = BasicBlock::Create(ctx.TheContext, "ifcont");
  ctx.Builder.CreateCondBr(CondV, ThenBB, ElseBB);
  // emit the value
  ctx.Builder.SetInsertPoint(ThenBB);

  Value *ThenV = Then->Codegen(ctx);
  if (ThenV == NULL)
    return NULL; // TODO: cleanup?

  ctx.Builder.CreateBr(MergeBB);
  // Codegen of 'Then' could switch the current block, update ThenBB for the
  // PHI.
  ThenBB = ctx.Builder.GetInsertBlock();

  // emit else block
  F->getBasicBlockList().push_back(ElseBB);
  ctx.Builder.SetInsertPoint(ElseBB);

  Value *ElseV = Else->Codegen(ctx);
  if (ElseV == NULL)
    return NULL;

  ctx.Builder.CreateBr(MergeBB);
  // Codegen for 'Else' can also switch the current block, get it
  ElseBB = ctx.Builder.GetInsertBlock();

  // emit merge block
  F->getBasicBlockList().push_back(MergeBB);
  ctx.Builder.SetInsertPoint(MergeBB);
  PHINode *PN =
      ctx.Builder.CreatePHI(Type::getDoubleTy(ctx.TheContext), 2, "iftmp");
  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
}

/* vim: set sw=2 sts=2  : */
