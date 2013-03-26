#ifndef MESH_TRIANGLE_H
#define MESH_TRIANGLE_H

#include "raytracer/primitives/Primitive.h"

namespace raytracer {
  class Mesh;

  class MeshTriangle : public Primitive {
  public:
    MeshTriangle(Mesh* mesh, int index0, int index1, int index2)
      : Primitive(), m_mesh(mesh), m_index0(index0), m_index1(index1), m_index2(index2) {}

    virtual BoundingBox boundingBox();

  protected:
    Mesh* m_mesh;
    int m_index0, m_index1, m_index2;
  };
}

#endif
