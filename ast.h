#ifndef _AST_H_
#define _AST_H_

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/PassManager.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/IRBuilder.h>

#include <string>
#include <vector>
#include <map>

#include "lexer.h"

class Executor {
public:
  llvm::LLVMContext &TheContext;
  llvm::IRBuilder<> Builder;
  llvm::Module *TheModule;
  llvm::FunctionPassManager *TheFPM;
  llvm::ExecutionEngine *TheEE;
  std::map<std::string, llvm::Value *> NamedValues;

  typedef double (*fptr)();

  Executor();              // TODO: free resources
  fptr Exec(Lexer &lexer); // returns a func-pointer
};

class ExprAST {
public:
  virtual ~ExprAST() {}
  virtual llvm::Value *Codegen(Executor &ctx) = 0;
};

// Expression for numeric values
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double val);
  virtual llvm::Value *Codegen(Executor &ctx);
};

// Expression for variable references
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &name);
  virtual llvm::Value *Codegen(Executor &ctx);
};

// Expressions for a unary operator
class UnaryExprAST : public ExprAST {
  Token::lexic_component Op;
  ExprAST *Expr;

public:
  UnaryExprAST(Token::lexic_component op, ExprAST *expr);
  virtual llvm::Value *Codegen(Executor &ctx);
};

// Expressions for a binary operator
class BinaryExprAST : public ExprAST {
  Token::lexic_component Op;
  ExprAST *LHS, *RHS;

public:
  BinaryExprAST(Token::lexic_component op, ExprAST *lhs, ExprAST *rhs);
  virtual llvm::Value *Codegen(Executor &ctx);
};

// Expression for function calls
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<ExprAST *> Args;

public:
  CallExprAST(const std::string &callee, std::vector<ExprAST *> &args);
  virtual llvm::Value *Codegen(Executor &ctx);
};

// This represents a function signature
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;

public:
  PrototypeAST(const std::string &name, const std::vector<std::string> &args);
  virtual llvm::Function *Codegen(Executor &ctx);
};

// This represents an actual function definition
class FunctionAST {
  PrototypeAST *Proto;
  ExprAST *Body;

public:
  FunctionAST(PrototypeAST *proto, ExprAST *body);
  virtual llvm::Function *Codegen(Executor &ctx);
};

// Conditional expressions
class IfExprAST : public ExprAST {
  ExprAST *Cond;
  ExprAST *Then;
  ExprAST *Else;

public:
  IfExprAST(ExprAST *cond, ExprAST *then, ExprAST *_else);
  virtual llvm::Value *Codegen(Executor &ctx);
};

// Parse a top level rule
llvm::Function *ParseNext(Lexer &lexer, Executor &ctx);

#endif // _AST_H_

/* vim: set sw=2 sts=2 : */
