#include <string>

class Exception {
public:
  inline Exception(const std::string& message, const std::string& file, int line)
    : m_message(message), m_file(file), m_line(line)
  {
  }
  
  inline const std::string& message() const { return m_message; }

private:
  std::string m_message;
  std::string m_file;
  int m_line;
};
