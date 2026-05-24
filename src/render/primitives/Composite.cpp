#include "render/State.h"
#include "render/primitives/Composite.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "core/geometry/Mesh.h"

#include <limits>

using namespace std;
using namespace render;

Composite::~Composite() {
}

const Primitive* Composite::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                      render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return nullptr;
  }

  const Primitive* hit = nullptr;
  double minDistance = numeric_limits<double>::infinity();

  for (const auto& i : m_primitives) {
    HitPointInterval candidate;
    auto primitive = i->intersect(ray, candidate, state);
    if (primitive) {
      hitPoints = hitPoints + candidate;

      double distance = candidate.minWithPositiveDistance().distance();
      if (distance < minDistance) {
        hit = primitive;
        minDistance = distance;
      }
    }
  }

  return hit;
}

bool Composite::intersects(const Rayd& ray, render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return false;
  }

  for (const auto& i : m_primitives) {
    if (i->intersects(ray, state))
      return true;
  }

  return false;
}

void Composite::forEachLeaf(std::shared_ptr<render::Material> inheritedMaterial,
                            const LeafVisitor& visitor) const {
  auto own = material();
  auto effective = own ? own : inheritedMaterial;

  for (const auto& primitive : m_primitives) {
    primitive->forEachLeaf(effective, visitor);
  }
}

void Composite::forEachLeafInBounds(const BoundsFilter& boundsFilter,
                                    std::shared_ptr<render::Material> inheritedMaterial,
                                    const LeafVisitor& visitor) const {
  if (!boundsFilter(boundingBox())) {
    return;
  }

  auto own = material();
  auto effective = own ? own : inheritedMaterial;

  for (const auto& primitive : m_primitives) {
    primitive->forEachLeafInBounds(boundsFilter, effective, visitor);
  }
}

BoundingBoxd Composite::calculateBoundingBox() const {
  BoundingBoxd b;
  for (const auto& i : m_primitives)
    b.include(i->boundingBox());
  return b;
}

std::shared_ptr<Mesh> Composite::tessellate(int lod) const {
  auto result = std::make_shared<Mesh>();
  int vertexOffset = 0;

  for (const auto& prim : m_primitives) {
    auto childMesh = prim->tessellate(lod);
    if (!childMesh)
      continue;

    for (const auto& v : childMesh->vertices())
      result->addVertex(v.point, v.normal, v.uv);

    for (const auto& face : childMesh->faces()) {
      Mesh::Face remapped;
      remapped.reserve(face.size());
      for (int idx : face)
        remapped.push_back(idx + vertexOffset);
      result->addFace(remapped);
    }

    vertexOffset += static_cast<int>(childMesh->vertices().size());
  }

  return result;
}
