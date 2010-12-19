#include "core/Exception.h"

#include <execinfo.h>
#include <cstdlib>
#include <iostream>

using namespace std;

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

void Exception::printBacktrace() {
  for (Backtrace::const_iterator i = m_backtrace.begin(); i != m_backtrace.end(); ++i) {
    cout << *i << endl;
  }
}