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

static AllocaInst *CreateEntryBlockAlloca(Kaleidoscope &ctx,
                                          const string &Var) {
  BasicBlock *EB = &ctx.Builder.GetInsertBlock()->getParent()->getEntryBlock();
  IRBuilder<> B(EB, EB->begin());
  return B.CreateAlloca(Type::getDoubleTy(ctx.TheContext), 0, Var.c_str());
}

// Op-Token => <precedence, associativity (-1 left, 1 right)> (llparser.cc)
extern map<Token, pair<int, int> > OperatorPrecedenceAssoc;

Kaleidoscope::Kaleidoscope()
    : TheContext(getGlobalContext()), Builder(TheContext) {
  InitializeNativeTarget();
  TheModule = new Module("Kaleidoscope", TheContext);
  // Create the JIT execution engine
  TheEE = EngineBuilder(TheModule).create();
  // setup a function level optimizer
  TheFPM = new FunctionPassManager(TheModule);
  TheFPM->add(new DataLayout(*TheEE->getDataLayout()));
  TheFPM->add(createBasicAliasAnalysisPass());
  TheFPM->add(createPromoteMemoryToRegisterPass());
  TheFPM->add(createInstructionCombiningPass());
  TheFPM->add(createReassociatePass());
  TheFPM->add(createGVNPass());
  TheFPM->add(createCFGSimplificationPass());
  TheFPM->doInitialization();

  // Initialize operator precedence
  OperatorPrecedenceAssoc[Token(Token::tokLT, "<")] = make_pair(10, -1);
  OperatorPrecedenceAssoc[Token(Token::tokMinus, "-")] = make_pair(20, -1);
  OperatorPrecedenceAssoc[Token(Token::tokPlus, "+")] = make_pair(20, -1);
  OperatorPrecedenceAssoc[Token(Token::tokMultiply, "*")] = make_pair(40, -1);
  OperatorPrecedenceAssoc[Token(Token::tokDivide, "/")] = make_pair(40, -1);
}

Kaleidoscope::fptr Kaleidoscope::Parse(Lexer &lexer) {
  // JIT the function returning a func ptr
  pair<bool, Function *> R = ParseNext(lexer, *this);
  if (R.first && R.second) {
    //R.second->dump();
    return (double(*)()) TheEE->getPointerToFunction(R.second);
  }
  return NULL;
}

// ----------------------------------------------------------------------
NumberExprAST::NumberExprAST(double val) : Val(val) {}

Value *NumberExprAST::Codegen(Kaleidoscope &ctx) {
  return ConstantFP::get(ctx.TheContext, APFloat(Val));
}

// ----------------------------------------------------------------------
VariableExprAST::VariableExprAST(const string &name) : Name(name) {}

Value *VariableExprAST::Codegen(Kaleidoscope &ctx) {
  Value *V = ctx.NamedValues[Name];
  if (V == NULL)
    return ValueError("Unknown variable name");
  // load the value
  return ctx.Builder.CreateLoad(V, Name.c_str());
}

// ----------------------------------------------------------------------
UnaryExprAST::UnaryExprAST(const Token &op, ExprAST *expr)
    : Op(op), Expr(expr) {}

Value *UnaryExprAST::Codegen(Kaleidoscope &ctx) {
  Value *V = Expr->Codegen(ctx);
  if (V == NULL)
    return NULL;
  // check for our unary -
  if (Op.lex_comp == Token::tokMinus)
    return ctx.Builder.CreateFNeg(V, "negtmp");
  // check for a user defined unary op
  Function *F = ctx.TheModule->getFunction("unary" + Op.lexem);
  if (F == NULL)
    return ValueError("Invalid unary operator");
  return ctx.Builder.CreateCall(F, V, "uniop");
}

// ----------------------------------------------------------------------
BinaryExprAST::BinaryExprAST(const Token &op, ExprAST *lhs, ExprAST *rhs)
    : Op(op), LHS(lhs), RHS(rhs) {}

Value *BinaryExprAST::Codegen(Kaleidoscope &ctx) {
  Value *L = LHS->Codegen(ctx);
  Value *R = RHS->Codegen(ctx);
  if (L == NULL || R == NULL)
    return NULL;

  switch (Op.lex_comp) {
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
    break; // must be a user defined op
  }
  // check for user defined operators
  Function *F = ctx.TheModule->getFunction("binary" + Op.lexem);
  if (F == NULL)
    return ValueError("Invalid binary operator");
  Value *Ops[2] = { L, R };
  return ctx.Builder.CreateCall(F, Ops, "binop");
}

// ----------------------------------------------------------------------
CallExprAST::CallExprAST(const string &callee, vector<ExprAST *> &args)
    : Callee(callee), Args(args) {}

Value *CallExprAST::Codegen(Kaleidoscope &ctx) {
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
PrototypeAST::PrototypeAST(const string &name, const vector<string> &args,
                           const Token &op, pair<int, int> opprecassoc)
    : Name(name), Args(args), Op(op), opPrecAssoc(opprecassoc) {}

// create allocas for all function arguments
void PrototypeAST::CreateArgumentAllocas(Kaleidoscope &ctx, Function *F) {
  Function::arg_iterator AI = F->arg_begin();
  for (unsigned idx = 0; idx != Args.size(); ++idx, ++AI) {
    // create an alloca for this variable
    AllocaInst *A = CreateEntryBlockAlloca(ctx, Args[idx]);
    // store the initial value into the alloca
    ctx.Builder.CreateStore(AI, A);
    // add args to variable-symbol-table
    ctx.NamedValues[Args[idx]] = A;
  }
}

// http://llvm.org/releases/3.3/docs/tutorial/LangImpl3.html#id4
Function *PrototypeAST::Codegen(Kaleidoscope &ctx) {
  Function *F = NULL;
  if ((F = ctx.TheModule->getFunction(Name)) == NULL) {
    // make the function type: double(double, double) ... etc.
    vector<Type *> DblArgs(Args.size(), Type::getDoubleTy(ctx.TheContext));
    FunctionType *FT = // returns a double, takes n-doubles, is not vararg
        FunctionType::get(Type::getDoubleTy(ctx.TheContext), DblArgs, false);
    // register our function in TheModule with name Name
    if (Name == "unary" || Name == "binary")
      F = Function::Create(FT, Function::ExternalLinkage, Name + Op.lexem,
                           ctx.TheModule);
    else
      F = Function::Create(FT, Function::ExternalLinkage, Name, ctx.TheModule);
  }

  if (!F->empty()) // check the function es a forward decl if it exists
    return FunctionError("Function cannot be redefined");
  if (F->arg_size() != Args.size()) // defined and empty => extern, check # args
    return FunctionError("Redefinition of function with wrong # of args");

  // set names for all arguments
  Function::arg_iterator AI = F->arg_begin();
  for (unsigned idx = 0; idx != Args.size(); ++AI, ++idx) {
    AI->setName(Args[idx]);
  }

  // check if prototype defines an operator => install precedence/associativity
  if (Name == "unary" || Name == "binary")
    OperatorPrecedenceAssoc[Op] = opPrecAssoc;

  return F;
}

// ----------------------------------------------------------------------
FunctionAST::FunctionAST(PrototypeAST *proto, ExprAST *body)
    : Proto(proto), Body(body) {}

Function *FunctionAST::Codegen(Kaleidoscope &ctx) {
  ctx.NamedValues.clear(); // clear scope
  Function *F = Proto->Codegen(ctx);
  if (F == NULL)
    return NULL;

  // Create a new basic block to start insertion into.
  BasicBlock *BB = BasicBlock::Create(ctx.TheContext, "entry", F);
  ctx.Builder.SetInsertPoint(BB);

  // add arguments to the symbol-table
  Proto->CreateArgumentAllocas(ctx, F);

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

Value *IfExprAST::Codegen(Kaleidoscope &ctx) {
  Value *CondV = Cond->Codegen(ctx);
  if (CondV == NULL)
    return NULL;

  // convert condition to a bool by comparing equal to 0.0
  CondV = ctx.Builder.CreateFCmpONE(
      CondV, ConstantFP::get(ctx.TheContext, APFloat(0.0)), "ifcond");

  // ask the builder for the current basic block,
  // the parent of this BB is the function holding it
  Function *F = ctx.Builder.GetInsertBlock()->getParent();
  BasicBlock *PreBB = ctx.Builder.GetInsertBlock();

  // create BBlocks for if/then/else inserting the "then" block
  // at the end of the F function, other blocks are yet not inserted
  BasicBlock *ThenBB = BasicBlock::Create(ctx.TheContext, "then", F);
  BasicBlock *ElseBB = NULL;
  if (Else)
    ElseBB = BasicBlock::Create(ctx.TheContext, "else");
  BasicBlock *MergeBB = BasicBlock::Create(ctx.TheContext, "ifcont");
  // where to resume (even though these blocks are yet not inserted)
  if (Else)
    ctx.Builder.CreateCondBr(CondV, ThenBB, ElseBB);
  else
    ctx.Builder.CreateCondBr(CondV, ThenBB, MergeBB);

  // Set the builder to emit at ThenBB
  ctx.Builder.SetInsertPoint(ThenBB);
  Value *ThenV = Then->Codegen(ctx);
  if (ThenV == NULL)
    return NULL;                 // TODO: cleanup?
  ctx.Builder.CreateBr(MergeBB); // once ThenBB is finished skip to Merge
  // The Codegen for the Then block may have changed the BB where code is
  // being emited, get a handle on this latest BB to build the PHI node
  ThenBB = ctx.Builder.GetInsertBlock();

  Value *ElseV = NULL;
  if (Else) {
    // Emit the Else block, re-set the insertion point (see prev comment)
    F->getBasicBlockList().push_back(ElseBB);
    ctx.Builder.SetInsertPoint(ElseBB);
    ElseV = Else->Codegen(ctx);
    if (ElseV == NULL)
      return NULL;
    ctx.Builder.CreateBr(MergeBB); // finalize ElseBB
    // Codegen for 'Else' may have switched BB, get handle to current
    ElseBB = ctx.Builder.GetInsertBlock();
  }

  // Emit the Merge block, re-set insertion point...
  F->getBasicBlockList().push_back(MergeBB);
  ctx.Builder.SetInsertPoint(MergeBB);
  // Create PHI node to return the conditional blocks
  ctx.Builder.SetInsertPoint(MergeBB);
  PHINode *PN =
      ctx.Builder.CreatePHI(Type::getDoubleTy(ctx.TheContext), 2, "iftmp");
  PN->addIncoming(ThenV, ThenBB);
  if (Else) {
    PN->addIncoming(ElseV, ElseBB);
  } else {
    Value *NullV = Constant::getNullValue(Type::getDoubleTy(ctx.TheContext));
    PN->addIncoming(NullV, PreBB);
  }

  return PN;
}

// ----------------------------------------------------------------------
// Output this as:
//   var = alloca double
//   ...
//   start = startexpr
//   store start -> var
//   goto loop
// loop:
//   ...
//   bodyexpr
//   ...
// loopend:
//   step = stepexpr
//   endcond = endexpr
//
//   curvar = load var
//   nextvar = curvar + step
//   store nextvar -> var
//   br endcond, loop, endloop
// outloop:

ForExprAST::ForExprAST(const std::string &varname, ExprAST *start, ExprAST *end,
                       ExprAST *step, ExprAST *body)
    : VarName(varname), Start(start), End(end), Step(step), Body(body) {}

Value *ForExprAST::Codegen(Kaleidoscope &ctx) {
  // Create the Alloca at the entry of the function and set it's start value
  AllocaInst *A = CreateEntryBlockAlloca(ctx, VarName);
  Value *StartV = Start->Codegen(ctx);
  if (StartV == NULL)
    return NULL;
  ctx.Builder.CreateStore(StartV, A);

  // get a handle on the function we're  inserting code into
  Function *F = ctx.Builder.GetInsertBlock()->getParent();
  BasicBlock *LoopBB = BasicBlock::Create(ctx.TheContext, "loop", F);
  ctx.Builder.CreateBr(LoopBB);

  ctx.Builder.SetInsertPoint(LoopBB);

  // if the loop scope shadows a variable, keep it's old value
  AllocaInst *OldVal = ctx.NamedValues[VarName];
  ctx.NamedValues[VarName] = A;

  // generate Body now that the loop variable is in scope
  Value *BodyV = Body->Codegen(ctx);
  if (BodyV == NULL)
    return NULL;

  Value *StepV = NULL;
  if (Step) {
    StepV = Step->Codegen(ctx);
    if (StepV == NULL)
      return NULL;
  } else {
    StepV = ConstantFP::get(ctx.TheContext, APFloat(1.0));
  }

  // compute end condition
  Value *EndV = End->Codegen(ctx);
  if (End == NULL)
    return NULL;

  // reload, increment, and restore the alloca (in case the body mutates the
  // variable)
  Value *CurVal = ctx.Builder.CreateLoad(A, VarName.c_str());
  Value *NextVal = ctx.Builder.CreateFAdd(CurVal, StepV, "nextvar");
  ctx.Builder.CreateStore(NextVal, A);

  // convert condition to bool by comparing to 0.0
  EndV = ctx.Builder.CreateFCmpONE(
      EndV, ConstantFP::get(ctx.TheContext, APFloat(0.0)), "loopcond");

  // insert the block coming after the loop
  BasicBlock *AfterBB = BasicBlock::Create(ctx.TheContext, "afterloop", F);
  ctx.Builder.CreateCondBr(EndV, LoopBB, AfterBB); // condition to keep looping
  // continue writing after the loop
  ctx.Builder.SetInsertPoint(AfterBB);

  // restore possibly shadowed var
  if (OldVal)
    ctx.NamedValues[VarName] = OldVal;
  else
    ctx.NamedValues.erase(VarName);

  // always return expr 0.0
  return Constant::getNullValue(Type::getDoubleTy(ctx.TheContext));
}

/* vim: set sw=2 sts=2  : */
