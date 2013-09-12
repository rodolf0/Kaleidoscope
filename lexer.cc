#include "lexer.h"

using namespace std;

Token::Token() : lex_comp(lexic_component(0)), lexem("") {}
Token::Token(lexic_component lex_comp, const string &lexem)
    : lex_comp(lex_comp), lexem(lexem) {}
bool Token::operator<(const Token &o) const { return lex_comp < o.lex_comp; }

Lexer::Lexer(istream &input)
    : input(input), current(Token(Token::tokEOF, "")) {}
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
    return Token(Token::tokEOF, "");

  // tokenize numbers
  if (isdigit(last)) {
    do {
      lexem.append(1, last);
      last = input.get();
    } while (isdigit(last));

    if (last != '.') {
      input.putback(last);
      return Token(Token::tokNumber, lexem);
    }

    // get decimal separator
    lexem.append(1, last);
    last = input.get();
    if (!isdigit(last)) {
      input.putback(last);
      return Token(Token::tokNumber, lexem);
    }

    // get decimal part
    do {
      lexem.append(1, last);
      last = input.get();
    } while (isdigit(last));
    input.putback(last);

    return Token(Token::tokNumber, lexem);
  }

  // tokenize commands/identifiers
  if (isalpha(last) || last == '_') {
    do {
      lexem.append(1, last);
      last = input.get();
    } while (isalpha(last) || last == '_');
    input.putback(last);

    if (lexem == "def")
      return Token(Token::tokDef, lexem);
    if (lexem == "extern")
      return Token(Token::tokExtern, lexem);
    if (lexem == "if")
      return Token(Token::tokIf, lexem);
    if (lexem == "then")
      return Token(Token::tokThen, lexem);
    if (lexem == "else")
      return Token(Token::tokElse, lexem);
    if (lexem == "for")
      return Token(Token::tokFor, lexem);
    if (lexem == "in")
      return Token(Token::tokIn, lexem);
    if (lexem == "binary")
      return Token(Token::tokBinary, lexem);
    if (lexem == "unary")
      return Token(Token::tokUnary, lexem);
    return Token(Token::tokId, lexem);
  }

  // don't know what this is
  return Token(Token::lexic_component(last), string(1, last));
}

/* vim: set sw=2 sts=2 : */
