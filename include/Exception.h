#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <string>
#include <list>

class Exception {
public:
  typedef std::list<std::string> Backtrace;
  
  inline Exception(const std::string& message, const std::string& file, int line)
    : m_message(message), m_file(file), m_line(line)
  {
    getBacktrace();
  }
  
  inline const std::string& message() const { return m_message; }
  
  void printBacktrace();

private:
  void getBacktrace();
  
  std::string m_message;
  std::string m_file;
  int m_line;
  Backtrace m_backtrace;
};

#endif
