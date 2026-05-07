#pragma once
#include <memory>

#include "render/primitives/MeshTriangle.h"
#include "core/math/Vector.h"

namespace render {
  class Composite;
  class Material;

  class SmoothMeshTriangle : public MeshTriangle {
  public:
    explicit SmoothMeshTriangle(const Mesh* mesh, int index0, int index1, int index2);

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, raytracer::State& state) const;
    virtual bool intersects(const Rayd& ray, raytracer::State& state) const;

    static void build(const Mesh* mesh, Composite* composite, std::shared_ptr<render::Material> material);

    /**
      * Returns a single-triangle Mesh with vertex positions and UVs copied from
      * the parent mesh. Each vertex carries its own per-vertex normal from the
      * source mesh, preserving smooth shading data. The @p lod parameter is
      * ignored.
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod) const;

  private:
    Vector3d interpolateNormal(float beta, float gamma) const;

    int k;
    double nu;
    double nv;
    double nd;
    double bnu;
    double bnv;
    double cnu;
    double cnv;
  };
}
