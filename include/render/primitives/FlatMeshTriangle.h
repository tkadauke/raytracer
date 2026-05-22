#pragma once
#include <memory>

#include "render/primitives/MeshTriangle.h"
#include "core/math/Vector.h"

namespace render {
  class Composite;
  class Material;

  /**
    * Mesh-backed triangle with a single normal for the whole face.
    *
    * The ray-triangle test first solves barycentric weights for the hit point:
    * `alpha = 1 - beta - gamma`, `beta`, and `gamma`. Those coordinates are
    * shared across the mesh pipeline: they decide whether the ray is inside
    * the triangle, they interpolate UVs, and they are the same coordinates a
    * rasterizer uses for per-fragment attributes. `FlatMeshTriangle` keeps the
    * normal flat by returning the same precomputed normal for every hit.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="mesh_triangle_interpolation.js"></script>
    * @endhtmlonly
    */
  class FlatMeshTriangle : public MeshTriangle {
  public:
    inline explicit FlatMeshTriangle(const Mesh* mesh, int index0, int index1, int index2)
      : MeshTriangle(mesh, index0, index1, index2)
    {
      m_normal = computeNormal();
    }

    static void build(const Mesh* mesh, Composite* composite, std::shared_ptr<render::Material> material);

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, render::State& state) const override;

    /**
      * Returns a single-triangle Mesh with vertex positions and UVs copied from
      * the parent mesh. All three vertices share the precomputed face normal
      * (@p m_normal). The @p lod parameter is ignored.
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod) const override;

  private:
    Vector3d computeNormal() const;

    Vector3d m_normal;
  };
}
