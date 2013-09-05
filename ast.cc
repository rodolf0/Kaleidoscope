#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/PassManager.h>
#include <iostream>
#include <map>

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

Module *TheModule = NULL;
FunctionPassManager *TheFPM = NULL;
static IRBuilder<> Builder(getGlobalContext());
// Symbol table for keeping variable definitions (no scope yet)
static map<string, Value *> NamedValues;

// ----------------------------------------------------------------------
NumberExprAST::NumberExprAST(double val) : Val(val) {}

Value *NumberExprAST::Codegen() {
  return ConstantFP::get(getGlobalContext(), APFloat(Val));
}

// ----------------------------------------------------------------------
VariableExprAST::VariableExprAST(const string &name) : Name(name) {}

Value *VariableExprAST::Codegen() {
  Value *v = NamedValues[Name];
  return v ? v : ValueError("Unknown variable name");
}

// ----------------------------------------------------------------------
UnaryExprAST::UnaryExprAST(Token::lexic_component op, ExprAST *expr)
    : Op(op), Expr(expr) {}

Value *UnaryExprAST::Codegen() {
  Value *V = Expr->Codegen();
  if (V == NULL)
    return NULL;
  if (Op != Token::tokMinus)
    return ValueError("Unknown unary operator");
  return Builder.CreateFNeg(V, "negtmp");
}

// ----------------------------------------------------------------------
BinaryExprAST::BinaryExprAST(Token::lexic_component op, ExprAST *lhs,
                             ExprAST *rhs)
    : Op(op), LHS(lhs), RHS(rhs) {}

Value *BinaryExprAST::Codegen() {
  Value *L = LHS->Codegen();
  Value *R = RHS->Codegen();
  if (L == NULL || R == NULL)
    return NULL;

  switch (Op) {
  case Token::tokLT:
    L = Builder.CreateFCmpULT(L, R, "cmptmp");
    // convert bool to double
    return Builder.CreateUIToFP(L, Type::getDoubleTy(getGlobalContext()),
                                "booltmp");
  case Token::tokPlus:
    return Builder.CreateFAdd(L, R, "addtmp");
  case Token::tokMinus:
    return Builder.CreateFSub(L, R, "subtmp");
  case Token::tokMultiply:
    return Builder.CreateFMul(L, R, "multmp");
  case Token::tokDivide:
    return Builder.CreateFDiv(L, R, "divtmp");
  default:
    return ValueError("Invalid binary operator");
  }
}

// ----------------------------------------------------------------------
CallExprAST::CallExprAST(const string &callee, vector<ExprAST *> &args)
    : Callee(callee), Args(args) {}

Value *CallExprAST::Codegen() {
  // lookup our function in the global module table
  Function *CalleeF = TheModule->getFunction(Callee);
  if (CalleeF == NULL)
    return ValueError("Unknown function referenced");
  if (CalleeF->arg_size() != Args.size())
    return ValueError("Incorrect # of arguments");

  vector<Value *> ArgsV;
  //for_each(Args.begin(), Args.end(), [](Value *arg) { ArgsV.push_back(arg);
  //});
  for (unsigned i = 0; i < Args.size(); i++) {
    ArgsV.push_back(Args[i]->Codegen());
    if (ArgsV.back() == NULL)
      return NULL;
  }
  return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

// ----------------------------------------------------------------------
PrototypeAST::PrototypeAST(const string &name, const vector<string> &args)
    : Name(name), Args(args) {}

// http://llvm.org/releases/3.3/docs/tutorial/LangImpl3.html#id4
Function *PrototypeAST::Codegen() {
  Function *F = NULL;
  if ((F = TheModule->getFunction(Name)) == NULL) {
    // make the function type: double(double, double) ... etc.
    vector<Type *> DblArgs(Args.size(), Type::getDoubleTy(getGlobalContext()));
    FunctionType *FT = // returns a double, takes n-doubles, is not vararg
        FunctionType::get(Type::getDoubleTy(getGlobalContext()), DblArgs,
                          false);
    // register our function in TheModule with name Name
    F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule);
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
    NamedValues[Args[idx]] = AI;
  }
  return F;
}

// ----------------------------------------------------------------------
FunctionAST::FunctionAST(PrototypeAST *proto, ExprAST *body)
    : Proto(proto), Body(body) {}

Function *FunctionAST::Codegen() {
  NamedValues.clear(); // only let some variables be in scope
  Function *F = Proto->Codegen();
  if (F == NULL)
    return NULL;

  // Create a new basic block to start insertion into.
  BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "entry", F);
  Builder.SetInsertPoint(BB);

  if (Value *RetVal = Body->Codegen()) {
    // finish off the function
    Builder.CreateRet(RetVal);
    //Validate the generated code, checking for consistency
    verifyFunction(*F);

    // Optimize the function of the Optimizer is available
    if (TheFPM != NULL)
      TheFPM->run(*F);

    return F;
  }
  // Error reading body, remove function from fsym-tab to let usr redefine it
  F->eraseFromParent();
  return NULL;
}

// ----------------------------------------------------------------------
IfExprAST::IfExprAST(ExprAST *cond, ExprAST *then, ExprAST *_else)
    : Cond(cond), Then(then), Else(_else) {}

Value *IfExprAST::Codegen() {
  Value *CondV = Cond->Codegen();
  if (CondV == NULL)
    return NULL;

  // convert condition to a bool by comparing equal to 0.0
  CondV = Builder.CreateFCmpONE(
      CondV, ConstantFP::get(getGlobalContext(), APFloat(0.0)), "ifcond");

  // ask the builder for the current basic block,
  // the parent of this BB is the function holding it
  Function *F = Builder.GetInsertBlock()->getParent();

  // create BBlocks for if/then/else cases
  // insert the "then" block at the end of the F function
  BasicBlock *ThenBB = BasicBlock::Create(getGlobalContext(), "then", F);
  BasicBlock *ElseBB = BasicBlock::Create(getGlobalContext(), "else");
  // where to resume after conditional
  BasicBlock *MergeBB = BasicBlock::Create(getGlobalContext(), "ifcont");
  Builder.CreateCondBr(CondV, ThenBB, ElseBB);
  // emit the value
  Builder.SetInsertPoint(ThenBB);

  Value *ThenV = Then->Codegen();
  if (ThenV == NULL)
    return NULL; // TODO: cleanup?

  Builder.CreateBr(MergeBB);
  // Codegen of 'Then' could switch the current block, update ThenBB for the
  // PHI.
  ThenBB = Builder.GetInsertBlock();

  // emit else block
  F->getBasicBlockList().push_back(ElseBB);
  Builder.SetInsertPoint(ElseBB);

  Value *ElseV = Else->Codegen();
  if (ElseV == NULL)
    return NULL;

  Builder.CreateBr(MergeBB);
  // Codegen for 'Else' can also switch the current block, get it
  ElseBB = Builder.GetInsertBlock();

  // emit merge block
  F->getBasicBlockList().push_back(MergeBB);
  Builder.SetInsertPoint(MergeBB);
  PHINode *PN =
      Builder.CreatePHI(Type::getDoubleTy(getGlobalContext()), 2, "iftmp");
  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
}

/* vim: set sw=2 sts=2  : */
