#include "ast.h"

using namespace std;

string ExprAST::String() { return ""; }

NumberExprAST::NumberExprAST(double val) : Val(val) {}
string NumberExprAST::String() { return to_string(Val); }

VariableExprAST::VariableExprAST(const string &name) : Name(name) {}
string VariableExprAST::String() { return Name; }

BinaryExprAST::BinaryExprAST(Token::lexic_component op, ExprAST *lhs,
                             ExprAST *rhs)
    : Op(op), LHS(lhs), RHS(rhs) {}
string BinaryExprAST::String() {
  return LHS->String() + " " + to_string(Op) + " " + RHS->String();
}

CallExprAST::CallExprAST(const string &callee, vector<ExprAST *> &args)
    : Callee(callee), Args(args) {}

PrototypeAST::PrototypeAST(const string &name, const vector<string> &args)
    : Name(name), Args(args) {}

FunctionAST::FunctionAST(PrototypeAST *proto, ExprAST *body)
    : Proto(proto), Body(body) {}

/* vim: set sw=2 sts=2l  : */
