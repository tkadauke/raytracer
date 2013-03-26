#ifndef SMOOTH_MESH_TRIANGLE_H
#define SMOOTH_MESH_TRIANGLE_H

#include "raytracer/primitives/MeshTriangle.h"
#include "core/math/Vector.h"

namespace raytracer {
  class SmoothMeshTriangle : public MeshTriangle {
  public:
    SmoothMeshTriangle(Mesh* mesh, int index0, int index1, int index2);

    virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);
    virtual bool intersects(const Ray& ray);

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

#endif
