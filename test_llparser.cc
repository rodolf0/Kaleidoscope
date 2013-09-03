#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <iostream>
#include "lexer.h"

using namespace std;

extern llvm::Module *TheModule;
void Parse(Lexer &lexer);

int main() {
  Lexer l(cin);
  l.Next(); // bootstrap the lexer

  TheModule = new llvm::Module("my cool jit module", llvm::getGlobalContext());

  Parse(l);

  TheModule->dump();

  return 0;
}

/* vim: set sw=2 sts=2 : */
