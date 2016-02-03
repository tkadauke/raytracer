#ifndef RAYTRACER_MESH_H
#define RAYTRACER_MESH_H

#include "core/math/Vector.h"
#include <vector>

namespace raytracer {
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

    void computeNormals(bool flip = false);
    void addSmoothTrianglesTo(Composite* composite, Material* material);
    void addFlatTrianglesTo(Composite* composite, Material* material);

  private:
    struct Private;
    Private* p;
  };
}

#endif
