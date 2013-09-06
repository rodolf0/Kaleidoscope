#include <iostream>
#include "ast.h"

using namespace std;

// Error handling
static ExprAST *ExprError(const char *error) {
  cerr << error << endl;
  return NULL;
}
static PrototypeAST *ProtoError(const char *error) {
  ExprError(error);
  return NULL;
}

// Parser functions policy: eat all tokens corresponding to the production

static ExprAST *ParseExpression(Lexer &lexer);

static ExprAST *ParseIfExpr(Lexer &lexer) {
  lexer.Next(); // eat 'if'
  ExprAST *Cond = ParseExpression(lexer);
  if (Cond == NULL)
    return NULL;
  // parse the branch for the condition being met
  if (lexer.Current().lex_comp != Token::tokThen)
    return ExprError("Expected 'then' in conditional");
  lexer.Next(); // eat 'then'
  ExprAST *Then = ParseExpression(lexer);
  if (Then == NULL)
    return NULL;
  // check for a branch for the condition not being met
  if (lexer.Current().lex_comp != Token::tokElse)
    return ExprError("Expected 'else' in conditional");
  lexer.Next(); // eat 'else'
  ExprAST *Else = ParseExpression(lexer);
  if (Else == NULL)
    return NULL;
  return new IfExprAST(Cond, Then, Else);
}

// primary ::= identifierexpr | numberexpr | parenexpr | '-' primary | ifexpr
static ExprAST *ParsePrimary(Lexer &lexer) {
  switch (lexer.Current().lex_comp) {
  // numberexpr
  case Token::tokNumber: {
    ExprAST *Num = new NumberExprAST(stod(lexer.Current().lexem));
    lexer.Next(); // eat number
    return Num;
  }

  // parenexpr
  case Token::tokOParen: {
    lexer.Next(); // eat '('
    ExprAST *expr = ParseExpression(lexer);
    if (expr == NULL)
      return NULL;
    if (lexer.Current().lex_comp != Token::tokCParen)
      return ExprError("Expected ')'");
    lexer.Next(); // eat ')'
    return expr;
  }

  // '-' primary
  case Token::tokMinus: {
    Token Op = lexer.Current();
    lexer.Next(); // eat '-'
    ExprAST *expr = ParsePrimary(lexer);
    if (expr == NULL)
      return NULL;
    return new UnaryExprAST(Op.lex_comp, expr);
  }

  // identifierexpr
  case Token::tokId: {
    string IdName = lexer.Current().lexem;
    // Check if this is a function call (eating the identifier)
    if (lexer.Next().lex_comp != Token::tokOParen)
      return new VariableExprAST(IdName);
    lexer.Next(); // eat '('
    vector<ExprAST *> Args;
    // Parse function arguments
    if (lexer.Current().lex_comp != Token::tokCParen) {
      while (true) {
        ExprAST *arg = ParseExpression(lexer);
        if (arg == NULL)
          return NULL;
        Args.push_back(arg);
        if (lexer.Current().lex_comp == Token::tokCParen)
          break;
        if (lexer.Current().lex_comp != Token::tokComma)
          return ExprError("Expected ')' or ',' in argument list");
        lexer.Next(); // eat ','
      }
    }
    lexer.Next(); // eat ')'
    return new CallExprAST(IdName, Args);
  }

  // ifexpr
  case Token::tokIf: { return ParseIfExpr(lexer); }

  default:
    return ExprError("Unknown token. Expected expression");
  }
}

static int TokenPrecedence(const Token &token) {
  switch (token.lex_comp) {
  case Token::tokLT:
    return 10;
  case Token::tokMinus:
  case Token::tokPlus:
    return 20;
  case Token::tokMultiply:
  case Token::tokDivide:
    return 40;
  default:
    return -1;
  }
}

// Operator associativity -1:left associative, 1:right
static int TokenAssoc(const Token &token) {
  switch (token.lex_comp) {
  default:
    return -1;
  }
}

// bioprhs ::= ('+' primary)*
static ExprAST *ParseBinOpRHS(Lexer &lexer, int ExprPrec, ExprAST *LHS) {
  while (true) {
    int TokenPrec = TokenPrecedence(lexer.Current());
    // check that binop binds as tightly as the current op, else we're done
    // if no binop is next then the -1 precedence will bail us out
    if (TokenPrec < ExprPrec)
      return LHS;
    // We now know this is a binary operator
    Token BinOp = lexer.Current();
    lexer.Next(); // eat binop and parse primary
    ExprAST *RHS = ParsePrimary(lexer);
    if (RHS == NULL)
      return NULL;
    // if BinOp binds less tightly with RHS than the next op (after RHS),
    // let that next operator take RHS as its LHS (OR if BinOp is right assoc)
    int NextPrec = TokenPrecedence(lexer.Current());
    if (TokenPrec < NextPrec ||
        (TokenPrec == NextPrec && TokenAssoc(BinOp) == 1)) {
      RHS = ParseBinOpRHS(lexer, NextPrec, RHS);
      if (RHS == NULL)
        return NULL;
    }
    // Merge LHS/RHS
    LHS = new BinaryExprAST(BinOp.lex_comp, LHS, RHS);
  }
}

// expression ::= primary binoprhs
static ExprAST *ParseExpression(Lexer &lexer) {
  ExprAST *LHS = ParsePrimary(lexer);
  if (LHS == NULL)
    return NULL;
  return ParseBinOpRHS(lexer, 0, LHS);
}

// prototype ::= id '(' id* ')'
// eg: foo(x y z)  # note there are no commas separating argnames
static PrototypeAST *ParseFuncProto(Lexer &lexer) {
  // Get the function name
  if (lexer.Current().lex_comp != Token::tokId)
    return ProtoError("Expected function name in prototype");
  string FnName = lexer.Current().lexem;
  // Get the parameter list (eating the identifier)
  if (lexer.Next().lex_comp != Token::tokOParen)
    return ProtoError("Expected '(' in prototype");
  // Get the list of argument names (eating the initial '(')
  vector<string> ArgNames;
  while (lexer.Next().lex_comp == Token::tokId)
    ArgNames.push_back(lexer.Current().lexem);
  if (lexer.Current().lex_comp != Token::tokCParen)
    return ProtoError("Expected ')' in prototype");
  lexer.Next(); // eat ')'
  return new PrototypeAST(FnName, ArgNames);
}

// definition ::= 'def' prototype expression
static FunctionAST *ParseFuncDef(Lexer &lexer) {
  lexer.Next(); // eat 'def'
  PrototypeAST *proto = ParseFuncProto(lexer);
  if (proto == NULL)
    return NULL;
  if (ExprAST *expr = ParseExpression(lexer))
    return new FunctionAST(proto, expr);
  return NULL;
}

// external ::= 'extern' prototype
static PrototypeAST *ParseExtern(Lexer &lexer) {
  lexer.Next(); // eat 'extern'
  return ParseFuncProto(lexer);
}

// toplevelexpr ::= expression
// allow to parse arbitrary expressions wrapped in a null function
static FunctionAST *ParseTopLevelExpr(Lexer &lexer) {
  if (ExprAST *expr = ParseExpression(lexer)) {
    PrototypeAST *proto = new PrototypeAST("", vector<string>());
    return new FunctionAST(proto, expr);
  }
  return NULL;
}

// top ::= definition | external | expression | ';'
llvm::Function *ParseNext(Lexer &lexer, Executor &ctx) {
  switch (lexer.Current().lex_comp) {
  case Token::tokEOF:
    break;

  case Token::tokSemicolon:
    lexer.Next(); // eat ';'
    break;

  case Token::tokDef:
    if (FunctionAST *F = ParseFuncDef(lexer)) {
      return F->Codegen(ctx);
    } else
      lexer.Next(); // skip token for error recovery
    break;

  case Token::tokExtern:
    if (PrototypeAST *P = ParseExtern(lexer)) {
      return P->Codegen(ctx);
    } else
      lexer.Next(); // skip token for error recovery
    break;

  default:
    if (FunctionAST *F = ParseTopLevelExpr(lexer)) {
      return F->Codegen(ctx);
    } else
      lexer.Next(); // skip token for error recovery
    break;
  }
  return NULL;
}

/* vim: set sw=2 sts=2 : */
