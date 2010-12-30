#ifndef PLY_ELEMENT_H
#define PLY_ELEMENT_H

#include "formats/ply/PlyProperty.h"
#include <iostream>
#include <string>
#include <vector>

class PlyElement {
public:
  PlyElement(std::istream& istream);
  PlyElement(const std::string& name, int count)
    : m_name(name), m_count(count) {}
  
  const std::string& name() const { return m_name; }
  int count() const { return m_count; }
  
  const std::vector<PlyProperty>& properties() const { return m_properties; }
  void addProperty(const PlyProperty& property) { m_properties.push_back(property); }

private:
  void parse(std::istream& istream);
  
  std::string m_name;
  int m_count;
  std::vector<PlyProperty> m_properties;
};

#endif
