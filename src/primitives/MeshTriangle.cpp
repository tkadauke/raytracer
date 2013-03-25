#include "primitives/MeshTriangle.h"
#include "primitives/Mesh.h"

BoundingBox MeshTriangle::boundingBox() {
  BoundingBox b;
  b.include(m_mesh->vertices[m_index0].point);
  b.include(m_mesh->vertices[m_index1].point);
  b.include(m_mesh->vertices[m_index2].point);
  return b;
}
