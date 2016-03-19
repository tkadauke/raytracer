#pragma once

#include "raytracer/primitives/MeshTriangle.h"
#include "core/math/Vector.h"

namespace raytracer {
  class SmoothMeshTriangle : public MeshTriangle {
  public:
    SmoothMeshTriangle(Mesh* mesh, int index0, int index1, int index2);

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;
    virtual bool intersects(const Rayd& ray, State& state) const;

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
