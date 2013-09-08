#include <iostream>

extern "C" // unmangled name
double putchard(double X) {
  std::cout << (char)X;
  return 0.0;
}

/* vim: set sw=2 sts=2 : */
