#include "raytracer/primitives/MeshTriangle.h"
#include "raytracer/primitives/Mesh.h"

using namespace raytracer;

BoundingBoxd MeshTriangle::calculateBoundingBox() const {
  BoundingBoxd b;
  b.include(m_mesh->vertices[m_index0].point);
  b.include(m_mesh->vertices[m_index1].point);
  b.include(m_mesh->vertices[m_index2].point);
  return b.grownByEpsilon();
}
