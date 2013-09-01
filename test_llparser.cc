#include <iostream>
#include "lexer.h"

using namespace std;

void Parse(Lexer &lexer);

int main() {
  Lexer l(cin);
  l.Next(); // bootstrap the lexer
  Parse(l);

  return 0;
}

/* vim: set sw=2 sts=2 : */
