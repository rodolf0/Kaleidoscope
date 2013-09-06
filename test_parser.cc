#include <iostream>
#include "ast.h"

using namespace std;

int main() {
  Lexer lexer(cin);
  Executor exec;
  lexer.Next(); // bootstrap the lexer

  while (lexer.Current().lex_comp != Token::tokEOF) {
    if (double (*FP)() = exec.Exec(lexer))
      cerr << FP() << endl;
  }

  return 0;
}

/* vim: set sw=2 sts=2 : */
