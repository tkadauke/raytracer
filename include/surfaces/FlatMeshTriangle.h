#ifndef FLAT_MESH_TRIANGLE_H
#define FLAT_MESH_TRIANGLE_H

#include "surfaces/MeshTriangle.h"
#include "math/Vector.h"

class FlatMeshTriangle : public MeshTriangle {
public:
  FlatMeshTriangle(Mesh* mesh, int index0, int index1, int index2)
    : MeshTriangle(mesh, index0, index1, index2)
  {
    m_normal = computeNormal();
  }
  
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);

private:
  Vector3d computeNormal() const;
  
  Vector3d m_normal;
};

#endif
