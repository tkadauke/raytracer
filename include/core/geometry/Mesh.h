#pragma once

#include "core/math/Vector.h"
#include <vector>

class Mesh {
public:
  struct Vertex {
    inline Vertex() {}
    inline explicit Vertex(const Vector3d& p, const Vector3d& n)
      : point(p), normal(n) {}
    Vector3d point;
    Vector3d normal;
  };
  typedef std::vector<int> Face;

  Mesh();
  ~Mesh();

  void computeNormals(bool flip = false);
  
  void addVertex(const Vertex& vertex);
  void addVertex(const Vector3d& point, const Vector3d& normal);
  void addFace(const Face& face);
  
  const std::vector<Vertex>& vertices() const;
  const std::vector<Face>& faces() const;

private:
  struct Private;
  std::unique_ptr<Private> p;
};
