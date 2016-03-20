#pragma once

#include <string>
#include <iostream>

class PlyProperty {
public:
  enum Type {
    INVALID,
    INT8,
    UINT8,
    INT16,
    UINT16,
    INT32,
    UINT32,
    FLOAT32,
    FLOAT64
  };
  
  explicit PlyProperty(std::istream& is);
  
  inline explicit PlyProperty(Type type, const std::string& name)
    : m_countType(INVALID),
      m_elementType(type),
      m_list(false),
      m_name(name)
  {
  }
  
  inline explicit PlyProperty(Type countType, Type elementType, const std::string& name)
    : m_countType(countType),
      m_elementType(elementType),
      m_list(true),
      m_name(name)
  {
  }

  inline const std::string& name() const {
    return m_name;
  }
  
  inline Type elementType() const {
    return m_elementType;
  }
  
  inline Type countType() const {
    return m_countType;
  }
  
  inline bool isList() const {
    return m_list;
  }
  
private:
  void parse(std::istream& is);
  Type mapType(const std::string& string);
  
  Type m_countType, m_elementType;
  bool m_list;
  std::string m_name;
};
