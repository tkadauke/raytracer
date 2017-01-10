#pragma once

#include "core/formats/ply/PlyElement.h"
#include <iostream>
#include <vector>

class Mesh;

class PlyProperty;

class PlyFile {
public:
  explicit PlyFile(std::istream& is);
  explicit PlyFile(std::istream& is, Mesh& mesh);
  
  void read(std::istream& is, Mesh& mesh);
  
  inline int elementCount() const {
    return m_elements.size();
  }
  
  inline const std::vector<PlyElement>& elements() const {
    return m_elements;
  }
  
private:
  void readHeader(std::istream& is);
  void parseFormat(std::istream& is);
  void parseElement(std::istream& is);
  void parseProperty(std::istream& is);
  
  void ignoreProperty(std::istream& is, const PlyProperty& property);

  std::vector<PlyElement> m_elements;
};
