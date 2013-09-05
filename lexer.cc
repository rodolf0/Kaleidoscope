#include "lexer.h"

using namespace std;

Token::Token() {}
Token::Token(const string &lexem, lexic_component lex_comp)
    : lexem(lexem), lex_comp(lex_comp) {}

Lexer::Lexer(istream &input)
  : input(input), current(Token("", Token::tokEOF)) {}
const Token &Lexer::Current() { return current; }

const Token &Lexer::Next() {
  current = next();
  return current;
}

Token Lexer::next() {
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
    input.putback(last);

    return Token(lexem, Token::tokNumber);
  }

  // tokenize commands/identifiers
  if (isalpha(last) || last == '_') {
    do {
      lexem.append(1, last);
      last = input.get();
    } while (isalpha(last) || last == '_');
    input.putback(last);

    if (lexem == "def")
      return Token(lexem, Token::tokDef);
    if (lexem == "extern")
      return Token(lexem, Token::tokExtern);
    if (lexem == "if")
      return Token(lexem, Token::tokIf);
    if (lexem == "then")
      return Token(lexem, Token::tokThen);
    if (lexem == "else")
      return Token(lexem, Token::tokElse);
    return Token(lexem, Token::tokId);
  }

  // don't know what this is
  return Token(string(1, last), static_cast<Token::lexic_component>(last));
}

/* vim: set sw=2 sts=2 : */
