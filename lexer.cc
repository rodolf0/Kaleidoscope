#include "lexer.h"

using namespace std;

Token::Token() : token(""), lex_comp(tokUnknown) {}
Token::Token(const string &token, lexic_component lex_comp)
    : token(token), lex_comp(lex_comp) {}

Lexer::Lexer(istream &input) : input(input) {}

Token Lexer::Next() {
  string lexem;
  int last = ' ';
  // consume all white space
  while (isspace(last))
    last = input.get();

  if (input.eof())
    return Token("", Token::tokEOF);

  // tokenize numbers
  if (isdigit(last)) {
    do {
      lexem.append(1, last);
      last = input.get();
    } while (isdigit(last));

    if (last != '.') {
      input.putback(last);
      return Token(lexem, Token::tokNumber);
    }

    // get decimal separator
    lexem.append(1, last);
    last = input.get();
    if (!isdigit(last)) {
      input.putback(last);
      return Token(lexem, Token::tokNumber);
    }

    // get decimal part
    do {
      lexem.append(1, last);
      last = input.get();
    } while (isdigit(last));

    return Token(lexem, Token::tokNumber);
  }

  // tokenize commands/identifiers
  if (isalpha(last) || last == '_') {
    do {
      lexem.append(1, last);
      last = input.get();
    } while (isalpha(last) || last == '_');

    if (lexem == "def")
      return Token(lexem, Token::tokDef);
    if (lexem == "extern")
      return Token(lexem, Token::tokExtern);
    return Token(lexem, Token::tokId);
  }

  // don't know what this is
  return Token(string(1, last), Token::tokUnknown);
}

#if 1
#include <iostream>
int main() {
  Lexer l = Lexer(cin);

  Token t;

  while ((t = l.Next()).lex_comp != Token::tokEOF) {
    switch (t.lex_comp) {
    case Token::tokNumber:
      cout << "Number: ";
      break;
    case Token::tokExtern:
    case Token::tokDef:
      cout << "Keyword: ";
      break;
    case Token::tokId:
      cout << "Id: ";
      break;
    default:
      cout << "Other: ";
      break;

    }
    cout << t.token << endl;
  }

  return 0;
}
#endif

/* vim: set sw=2 sts=2 : */
