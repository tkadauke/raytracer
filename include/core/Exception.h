#pragma once

#include <string>
#include <list>

class Exception {
public:
  typedef std::list<std::string> Backtrace;
  
  inline explicit Exception(const std::string& message, const std::string& file, int line)
    : m_message(message), m_file(file), m_line(line)
  {
    getBacktrace();
  }
  
  inline const std::string& message() const {
    return m_message;
  }
  
  inline const std::string& file() const {
    return m_file;
  }
  
  inline int line() const {
    return m_line;
  }
  
  void printBacktrace() const;

private:
  void getBacktrace();
  
  std::string m_message;
  std::string m_file;
  int m_line;
  Backtrace m_backtrace;
};

void printBacktrace();
void printInterceptedBacktrace();
