#ifndef MESH_H
#define MESH_H

#include "math/Vector.h"
#include <vector>

class Composite;
class Material;

class Mesh {
public:
  struct Vertex {
    Vertex() {}
    Vertex(const Vector3d& p, const Vector3d& n)
      : point(p), normal(n) {}
    Vector3d point;
    Vector3d normal;
  };
  typedef std::vector<int> Face;
  
  std::vector<Vertex> vertices;
  std::vector<Face> faces;
  
  Mesh();
  ~Mesh();
  
  void computeNormals();
  void addSmoothTrianglesTo(Composite* composite, Material* material);
  void addFlatTrianglesTo(Composite* composite, Material* material);

private:
  class Private;
  Private* p;
};

#endif
