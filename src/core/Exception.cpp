#include "core/Exception.h"

#include <execinfo.h>
#include <dlfcn.h>
#include <cxxabi.h>

#include <iostream>
#include <cstdlib>

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

// Uncomment the following line to get a backtrace on SIGSEGV in the CLI
static Trap sigsegv(SIGSEGV, crashHandler);

// This is for intercepting exceptions thrown by the std library.
namespace {
  void * last_frames[20];
  size_t last_size;
  std::string exception_name;

  std::string demangle(const char *name) {
    int status;
    std::unique_ptr<char,void(*)(void*)> realname(abi::__cxa_demangle(name, 0, 0, &status), &std::free);
    return status ? "failed" : &*realname;
  }
}

extern "C" {
  void __cxa_throw(void *ex, std::type_info *info, void (*dest)(void *)) {
    exception_name = demangle(reinterpret_cast<const std::type_info*>(info)->name());
    last_size = backtrace(last_frames, sizeof last_frames/sizeof(void*));

    static void (*const rethrow)(void*,void*,void(*)(void*)) = (void (*const)(void*,void*,void(*)(void*)))dlsym(RTLD_NEXT, "__cxa_throw");
    rethrow(ex,info,dest);
    
    // This is here because this function is declared noreturn
    throw 0;
  }
}

void printInterceptedBacktrace() {
  std::cerr << "Caught a: " << exception_name << std::endl;
  backtrace_symbols_fd(last_frames, last_size, 2);
}
