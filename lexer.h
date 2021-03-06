#ifndef _LEXER_H_
#define _LEXER_H_

#include <istream>
#include <string>

class Token {
public:
  typedef enum lexic_component {
    tokEOF = -1,
    // commands
    tokDef = -2,
    tokExtern = -3,
    // primary
    tokId = -4,
    tokNumber = -5,
    // control
    tokIf = -6,
    tokThen = -7,
    tokElse = -8,
    tokFor = -9,
    tokIn = -10,
    tokBinary = -11,
    tokUnary = -12,
    // explicitly enumerate some used by parser
    tokSemicolon = ';',
    tokOParen = '(',
    tokCParen = ')',
    tokLT = '<',
    tokComma = ',',
    tokPlus = '+',
    tokMinus = '-',
    tokMultiply = '*',
    tokDivide = '/',
    tokAssign = '=',
  } lexic_component;

  lexic_component lex_comp;
  std::string lexem;

  Token(); // Null value is lex_comp = 0, lexem = ""
  Token(lexic_component lex_comp, const std::string &lexem);

  bool operator<(const Token &o) const;
};

class Lexer {
  std::istream &input;
  Token current;
  Token next();

public:
  Lexer(std::istream &input);
  const Token &Next();
  const Token &Current();
};

#endif // _LEXER_H_

/* vim: set sw=2 sts=2 : */
