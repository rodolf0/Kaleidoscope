#include <istream>
#include <string>
#include <cctype>

using namespace std;

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

  string token;
  lexic_component lex_comp;

  Token();
  Token(const string &token, lexic_component lex_comp);
};

Token::Token() : token(""), lex_comp(tokUnknown) {}
Token::Token(const string &token, lexic_component lex_comp)
    : token(token), lex_comp(lex_comp) {}

class Lexer {
private:
  istream &input;

public:
  Lexer(istream &input);
  Token Next();
};

#include <sstream>

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

  // tokenize commands

  // don't know what this is
  return Token("", Token::tokUnknown);
}

#include <iostream>

int main() {
  Lexer l = Lexer(cin);

  Token t;

  while ((t = l.Next()).lex_comp != Token::tokEOF) {
    switch (t.lex_comp) {
      case Token::tokNumber:
        cout << "Number: ";
        break;
      default:
        cout << "Other: ";
        break;

    }
    cout << t.token << endl;
  }

  return 0;
}

/* vim: set sw=2 sts=2 : */
