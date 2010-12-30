#ifndef SMOOTH_MESH_TRIANGLE_H
#define SMOOTH_MESH_TRIANGLE_H

#include "surfaces/MeshTriangle.h"
#include "math/Vector.h"

class SmoothMeshTriangle : public MeshTriangle {
public:
  SmoothMeshTriangle(Mesh* mesh, int index0, int index1, int index2)
    : MeshTriangle(mesh, index0, index1, index2) {}
  
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);

private:
  Vector3d interpolateNormal(float beta, float gamma) const;
};

#endif
