#pragma once

#include <string>
#include <list>

/**
  * Base class for exception. This class holds the message, and the file and
  * line where the Exception occurred. It will also capture the backtrace at the
  * time of construction.
  */
class Exception {
public:
  typedef std::list<std::string> Backtrace;
  
  /**
    * Constructor. Creates an Exception with the given @p message. For @p file
    * and @p line, you can use the built-in __FILE__ and __LINE__ macros,
    * respectively. This constructor will capture the current stack trace, which
    * can be printed using printBacktrace().
    */
  inline explicit Exception(const std::string& message, const std::string& file, int line)
    : m_message(message),
      m_file(file),
      m_line(line)
  {
    getBacktrace();
  }
  
  /**
    * @returns a const-reference to the exception's message.
    */
  inline const std::string& message() const {
    return m_message;
  }
  
  /**
    * @returns a const-reference to the file name where the exception happened.
    */
  inline const std::string& file() const {
    return m_file;
  }
  
  /**
    * @returns the line number where the exception happened.
    */
  inline int line() const {
    return m_line;
  }
  
  /**
    * Prints the backtrace to stdout, which was current during this exception's
    * construction.
    */
  void printBacktrace() const;

private:
  void getBacktrace();
  
  std::string m_message;
  std::string m_file;
  int m_line;
  Backtrace m_backtrace;
};

/**
  * Dumps the current stack trace to stderr.
  */
void printBacktrace();
