#pragma once

#include "core/formats/ply/PlyElement.h"
#include <iostream>
#include <vector>

namespace raytracer {
  class Mesh;
}

class PlyProperty;

class PlyFile {
public:
  PlyFile(std::istream& is);
  PlyFile(std::istream& is, raytracer::Mesh& mesh);
  
  void read(std::istream& is, raytracer::Mesh& mesh);
  
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
