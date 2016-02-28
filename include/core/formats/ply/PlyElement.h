#pragma once

#include "core/formats/ply/PlyProperty.h"
#include <iostream>
#include <string>
#include <vector>

class PlyElement {
public:
  PlyElement(std::istream& istream);
  inline PlyElement(const std::string& name, int count)
    : m_name(name),
      m_count(count)
  {
  }
  
  inline const std::string& name() const {
    return m_name;
  }
  
  inline int count() const {
    return m_count;
  }
  
  inline const std::vector<PlyProperty>& properties() const {
    return m_properties;
  }
  
  inline void addProperty(const PlyProperty& property) {
    m_properties.push_back(property);
  }
  
private:
  void parse(std::istream& istream);
  
  std::string m_name;
  int m_count;
  std::vector<PlyProperty> m_properties;
};
