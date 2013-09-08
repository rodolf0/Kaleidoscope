#include <iostream>
#include "ast.h"

using namespace std;

int main() {
  Lexer lexer(cin);
  Kaleidoscope K;

  lexer.Next(); // bootstrap the lexer
  while (lexer.Current().lex_comp != Token::tokEOF) {
    if (double(*FP)() = K.Parse(lexer)) {
      cout << ">> " << FP() << endl;
    }
  }

  return 0;
}

/* vim: set sw=2 sts=2 : */
