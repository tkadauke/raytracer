#pragma once

#include "raytracer/primitives/MeshTriangle.h"
#include "core/math/Vector.h"

namespace raytracer {
  class FlatMeshTriangle : public MeshTriangle {
  public:
    FlatMeshTriangle(Mesh* mesh, int index0, int index1, int index2)
      : MeshTriangle(mesh, index0, index1, index2)
    {
      m_normal = computeNormal();
    }

    virtual Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints);

  private:
    Vector3d computeNormal() const;

    Vector3d m_normal;
  };
}
