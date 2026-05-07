#pragma once
#include <memory>

#include "render/primitives/MeshTriangle.h"
#include "core/math/Vector.h"

namespace render {
  class Composite;
  class Material;

  class FlatMeshTriangle : public MeshTriangle {
  public:
    inline explicit FlatMeshTriangle(const Mesh* mesh, int index0, int index1, int index2)
      : MeshTriangle(mesh, index0, index1, index2)
    {
      m_normal = computeNormal();
    }

    static void build(const Mesh* mesh, Composite* composite, std::shared_ptr<render::Material> material);

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, raytracer::State& state) const;

    /**
      * Returns a single-triangle Mesh with vertex positions and UVs copied from
      * the parent mesh. All three vertices share the precomputed face normal
      * (@p m_normal). The @p lod parameter is ignored.
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod) const;

  private:
    Vector3d computeNormal() const;

    Vector3d m_normal;
  };
}
