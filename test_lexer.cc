#include <iostream>
#include "lexer.h"

using namespace std;

int main() {
  Lexer l(cin);

  Token t;
  while ((t = l.Next()).lex_comp != Token::tokEOF)
    cerr << "Lexed: " << t.lexem << " as " << t.lex_comp << endl;

  return 0;
}

/* vim: set sw=2 sts=2 : */
