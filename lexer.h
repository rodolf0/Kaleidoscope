#ifndef _LEXER_H_
#define _LEXER_H_

#include <istream>
#include <string>

class Token {
public:
  typedef enum lexic_component {
    // commands
    tokDef,
    tokExtern,
    // primary
    tokId,
    tokNumber,
    // misc
    tokEOF,
    tokUnknown
  } lexic_component;

  std::string token;
  lexic_component lex_comp;

  Token();
  Token(const std::string &token, lexic_component lex_comp);
};

class Lexer {
private:
  std::istream &input;

public:
  Lexer(std::istream &input);
  Token Next();
};

#endif // _LEXER_H_

/* vim: set sw=2 sts=2 : */
