#ifndef OBJECT_H
#define OBJECT_H

#include <string>
#include <list>

class Object {
public:
  Object();
  virtual ~Object();
  
  inline const std::string& name() const { return m_name; }
  inline void setName(const std::string& name) { m_name = name; }
  
private:
  std::string m_name;
};

#endif
