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

// Op-Token => <precedence, associativity (-1 left, 1 right)>
map<Token, pair<int, int> > OperatorPrecedenceAssoc;
int OpPrec(const Token &op) {
  if (OperatorPrecedenceAssoc.find(op) == OperatorPrecedenceAssoc.end())
    return -1;
  return OperatorPrecedenceAssoc[op].first;
}
int OpAssoc(const Token &op) {
  if (OperatorPrecedenceAssoc.find(op) == OperatorPrecedenceAssoc.end())
    return -1;
  return OperatorPrecedenceAssoc[op].second;
}

bool checkValidOp(const string &lexem) {
  const string valid[] = { "!", "@", ":", "#", "$", "%", "^", "&", "|", ".",
                           "?" };
  for (unsigned i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i)
    if (lexem == valid[i])
      return true;
  return false;
}

// Parser functions policy: eat all tokens corresponding to the production

static ExprAST *ParseExpression(Lexer &lexer);
static ExprAST *ParsePrimary(Lexer &lexer);

// ifexpr ::= 'if' expression 'then' expression ('else' expression)?
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
  // check if there's an else clause
  if (lexer.Current().lex_comp != Token::tokElse)
    return new IfExprAST(Cond, Then, NULL);
  lexer.Next(); // eat 'else'
  ExprAST *Else = ParseExpression(lexer);
  if (Else == NULL)
    return NULL;
  return new IfExprAST(Cond, Then, Else);
}

// forexpr ::= 'for' id '=' expr ',' expr (',' expr)? 'in' expression
static ExprAST *ParseForExpr(Lexer &lexer) {
  lexer.Next(); // eat 'for'
  if (lexer.Current().lex_comp != Token::tokId)
    return ExprError("Expected identifier in for-expression");
  string LoopId = lexer.Current().lexem;
  if (lexer.Next().lex_comp != Token::tokAssign)
    return ExprError("Expected '=' after Id in for-expression");
  lexer.Next(); // eat '='
  ExprAST *Start = ParseExpression(lexer);
  if (Start == NULL)
    return NULL;
  if (lexer.Current().lex_comp != Token::tokComma)
    return ExprError("Expected ',' after for start expression");
  lexer.Next(); // eat ','
  ExprAST *End = ParseExpression(lexer);
  if (Start == NULL)
    return NULL;
  ExprAST *Step = NULL;
  if (lexer.Current().lex_comp == Token::tokComma) {
    lexer.Next(); // eat ','
    Step = ParseExpression(lexer);
    if (Step == NULL)
      return NULL;
  }
  if (lexer.Current().lex_comp != Token::tokIn)
    return ExprError("Expected 'in' after for end/step expression");
  lexer.Next(); // eat 'in'
  ExprAST *Body = ParseExpression(lexer);
  if (Body == NULL)
    return NULL;
  return new ForExprAST(LoopId, Start, End, Step, Body);
}

// unary ::= primary | '!' unary
static ExprAST *ParseUnary(Lexer &lexer) {
  if (!checkValidOp(lexer.Current().lexem))
    return ParsePrimary(lexer);
  // it's a unary op
  Token Op = lexer.Current();
  lexer.Next(); // eat op
  if (ExprAST *operand = ParseUnary(lexer))
    return new UnaryExprAST(Op, operand);
  return NULL;
}

// primary ::= idexpr | numexpr | parenexpr | '-' primary | ifexpr | forexpr
// NOTE because of the way we implement op-precedence grammar unary operators
// have greater precedence than binary ones
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

  // '-' primary
  case Token::tokMinus: {
    Token Op = lexer.Current();
    lexer.Next(); // eat '-'
    ExprAST *expr = ParsePrimary(lexer);
    if (expr == NULL)
      return NULL;
    return new UnaryExprAST(Op, expr);
  }

  // ifexpr
  case Token::tokIf: { return ParseIfExpr(lexer); }
  // forexpr
  case Token::tokFor: { return ParseForExpr(lexer); }

  default:
    return ExprError("Unknown token. Expected expression");
  }
}

// bioprhs ::= ('+' unary)*
static ExprAST *ParseBinOpRHS(Lexer &lexer, int ExprPrec, ExprAST *LHS) {
  while (true) {
    int TokenPrec = OpPrec(lexer.Current());
    // check that binop binds as tightly as the current op, else we're done
    // if no binop is next then the -1 precedence will bail us out
    if (TokenPrec < ExprPrec)
      return LHS;
    // We now know this is a binary operator
    Token BinOp = lexer.Current();
    lexer.Next(); // eat binop and parse primary
    ExprAST *RHS = ParseUnary(lexer);
    if (RHS == NULL)
      return NULL;
    // if BinOp binds less tightly with RHS than the next op (after RHS),
    // let that next operator take RHS as its LHS (OR if BinOp is right assoc)
    int NextPrec = OpPrec(lexer.Current());
    if (TokenPrec < NextPrec ||
        (TokenPrec == NextPrec && OpAssoc(BinOp) == 1)) {
      RHS = ParseBinOpRHS(lexer, NextPrec, RHS);
      if (RHS == NULL)
        return NULL;
    }
    // Merge LHS/RHS
    LHS = new BinaryExprAST(BinOp, LHS, RHS);
  }
}

// expression ::= unary binoprhs
static ExprAST *ParseExpression(Lexer &lexer) {
  ExprAST *LHS = ParseUnary(lexer);
  if (LHS == NULL)
    return NULL;
  return ParseBinOpRHS(lexer, 0, LHS);
}

// prototype ::= id '(' id* ')'
//           ::= 'binary' id num (left|right)? '(' id id ')'
//           ::= 'unary' id '(' id ')' // no precedence for unary ops...
static PrototypeAST *ParseFuncProto(Lexer &lexer) {
  string FnName = "";
  Token Op;
  unsigned BinPrec = 30; // default precedence
  int Assoc = -1;
  switch (lexer.Current().lex_comp) {
  default:
    return ProtoError(
        "Expected function name or 'binary' or 'unary' in prototype");
  case Token::tokId:
    FnName = lexer.Current().lexem;
    lexer.Next(); // eat id
    break;

  case Token::tokBinary:
    FnName = "binary";
    if (!checkValidOp(lexer.Next().lexem))
      return ProtoError("Expected binary operator");
    Op = lexer.Current();
    if (lexer.Next().lex_comp != Token::tokNumber)
      return ProtoError("Expected binary op precedence");
    BinPrec = (unsigned) stoi(lexer.Current().lexem);
    if (BinPrec < 1 || BinPrec > 100)
      return ProtoError("Expected precedence between 1 and 100");
    // parse binary operator associativity
    if (lexer.Next().lex_comp == Token::tokId) {
      if (lexer.Current().lexem == "left")
        Assoc = 1;
      else if (lexer.Current().lexem == "right")
        Assoc = -1;
      else
        return ProtoError("Expected 'left' or 'right' associativity");
      lexer.Next(); // eat 'left'|'right'
    }
    break;

  case Token::tokUnary:
    FnName = "unary";
    if (!checkValidOp(lexer.Next().lexem))
      return ProtoError("Expected unary operator");
    Op = lexer.Current();
    lexer.Next(); // eat op
    break;
  }

  // Get the parameter list (eating the identifier)
  if (lexer.Current().lex_comp != Token::tokOParen)
    return ProtoError("Expected '(' in prototype");
  // Get the list of argument names (eating the initial '(')
  vector<string> ArgNames;
  while (lexer.Next().lex_comp == Token::tokId)
    ArgNames.push_back(lexer.Current().lexem);
  if (lexer.Current().lex_comp != Token::tokCParen)
    return ProtoError("Expected ')' in prototype");
  lexer.Next(); // eat ')'

  if ((FnName == "binary" && ArgNames.size() != 2) ||
      (FnName == "unary" && ArgNames.size() != 1))
    return ProtoError("Invalid number of operands for operator");

  return new PrototypeAST(FnName, ArgNames, Op, make_pair(BinPrec, Assoc));
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
// returns true for success, false if any parse errors, and a F if applicable
pair<bool, llvm::Function *> ParseNext(Lexer &lexer, Kaleidoscope &ctx) {
  switch (lexer.Current().lex_comp) {
  case Token::tokEOF:
    return make_pair(true, (llvm::Function *)NULL);
    break;

  case Token::tokSemicolon:
    lexer.Next(); // ignore top-level ';'
    return make_pair(true, (llvm::Function *)NULL);

  case Token::tokDef:
    if (FunctionAST *F = ParseFuncDef(lexer)) {
      F->Codegen(ctx);
      return make_pair(true, (llvm::Function *)NULL);
    }
    break;

  case Token::tokExtern:
    if (PrototypeAST *P = ParseExtern(lexer)) {
      P->Codegen(ctx);
      return make_pair(true, (llvm::Function *)NULL);
    }
    break;

  default:
    if (FunctionAST *F = ParseTopLevelExpr(lexer))
      return make_pair(true, F->Codegen(ctx));
    break;
  }

  lexer.Next(); // skip token for error recovery
  return make_pair(false, (llvm::Function *)NULL);
}

/* vim: set sw=2 sts=2 : */
