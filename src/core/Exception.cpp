#include "core/Exception.h"

#include <execinfo.h>

#include <csignal>
#include <cstdlib>
#include <iostream>

using namespace std;

// This is for exceptions thrown in our own code
void Exception::getBacktrace() {
  void *array[200];
  size_t size;
  char **strings;
  size_t i;

  size = backtrace(array, 200);
  strings = backtrace_symbols(array, size);

  for (i = 0; i < size; i++)
    m_backtrace.push_back(strings[i]);

  free(strings);
}

void Exception::printBacktrace() const {
  for (const auto& i : m_backtrace) {
    cout << i << endl;
  }
}

// This is for printing a backtrace on a fatal signal.
class Trap {
  typedef void (*Handler)(int);
public:
  Trap(int sig, Handler handler)
  {
    signal(sig, handler);
  }
};

void printBacktrace() {
  void *array[200];
  size_t size;

  size = backtrace(array, 200);

  backtrace_symbols_fd(array, size, 2);
}

static void crashHandler(int sig) {
  cerr << "Error: signal " << sig << ":" << endl;
  printBacktrace();
  exit(1);
}

// SIGSEGV trap that dumps a stack trace on the way down. Registered as a
// global so the handler is installed during static init.
static Trap sigsegv(SIGSEGV, crashHandler);
