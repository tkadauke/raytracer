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
  
  PlyProperty(std::istream& is);
  PlyProperty(Type type, const std::string& name)
    : m_elementType(type), m_list(false), m_name(name) {}
  PlyProperty(Type countType, Type elementType, const std::string& name)
    : m_countType(countType), m_elementType(elementType), m_list(true), m_name(name) {}

  const std::string& name() const { return m_name; }
  Type elementType() const { return m_elementType; }
  Type countType() const { return m_countType; }
  bool isList() const { return m_list; }

private:
  void parse(std::istream& is);
  Type mapType(const std::string& string);
  
  Type m_countType, m_elementType;
  bool m_list;
  std::string m_name;
};
