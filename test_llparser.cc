#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/PassManager.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/LinkAllPasses.h>
#include <iostream>
#include "lexer.h"

using namespace std;

extern llvm::Module *TheModule;
extern llvm::FunctionPassManager *TheFPM;
void Parse(Lexer &lexer);

int main() {
  TheModule = new llvm::Module("my cool jit module", llvm::getGlobalContext());

  // Create the JIT, this takes ownership of the module.
  string ErrStr;
  llvm::ExecutionEngine *TheEE =
      llvm::EngineBuilder(TheModule).setErrorStr(&ErrStr).create();
  if (TheEE == NULL) {
    cerr << "Failed to create Execution Engine: " << ErrStr << endl;
    return 1;
  }

  // setup a function level optimizer
  TheFPM = new llvm::FunctionPassManager(TheModule);
  TheFPM->add(new llvm::DataLayout(*TheEE->getDataLayout()));
  TheFPM->add(llvm::createBasicAliasAnalysisPass());
  TheFPM->add(llvm::createInstructionCombiningPass());
  TheFPM->add(llvm::createReassociatePass());
  TheFPM->add(llvm::createGVNPass());
  TheFPM->add(llvm::createCFGSimplificationPass());
  TheFPM->doInitialization();

  Lexer l(cin);
  l.Next(); // bootstrap the lexer
  Parse(l);

  //TheModule->dump();

  return 0;
}

/* vim: set sw=2 sts=2 : */
