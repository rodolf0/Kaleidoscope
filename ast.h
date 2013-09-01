#ifndef _AST_H_
#define _AST_H_

#include <string>
#include <vector>
#include "lexer.h"

class ExprAST {
public:
  virtual ~ExprAST() {}
};

// Expression for numeric values
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double val) : Val(val) {}
};

// Expression for variable references
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &name) : Name(name) {}
};

// Expressions for a binary operator
class BinaryExprAST : public ExprAST {
  Token::lexic_component Op;
  ExprAST *LHS, *RHS;

public:
  BinaryExprAST(Token::lexic_component op, ExprAST *lhs, ExprAST *rhs)
      : Op(op), LHS(lhs), RHS(rhs) {}
};

// Expression for function calls
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<ExprAST *> Args;

public:
  CallExprAST(const std::string &callee, std::vector<ExprAST *> &args)
      : Callee(callee), Args(args) {}
};

// This represents a function signature
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;

public:
  PrototypeAST(const std::string &name, const std::vector<std::string> &args)
      : Name(name), Args(args) {}
};

// This represents an actual function definition
class FunctionAST {
  PrototypeAST *Proto;
  ExprAST *Body;

public:
  FunctionAST(PrototypeAST *proto, ExprAST *body) : Proto(proto), Body(body) {}
};

#endif // _AST_H_

/* vim: set sw=2 sts=2 : */
