#pragma once

#include "raytracer/primitives/MeshTriangle.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Composite;
  class Material;

  class FlatMeshTriangle : public MeshTriangle {
  public:
    inline explicit FlatMeshTriangle(const Mesh* mesh, int index0, int index1, int index2)
      : MeshTriangle(mesh, index0, index1, index2)
    {
      m_normal = computeNormal();
    }

    static void build(const Mesh* mesh, Composite* composite, std::shared_ptr<Material> material);

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;

  private:
    Vector3d computeNormal() const;

    Vector3d m_normal;
  };
}
