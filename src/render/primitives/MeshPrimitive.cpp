#include "render/primitives/MeshPrimitive.h"

#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "render/State.h"
#include "render/primitives/FlatMeshTriangle.h"
#include "render/primitives/SmoothMeshTriangle.h"

#include <limits>
#include <utility>

using namespace render;

MeshPrimitive::MeshPrimitive(Mesh mesh, NormalMode normalMode)
    : m_mesh(std::make_shared<Mesh>(std::move(mesh))),
      m_normalMode(normalMode) {
  buildLeaves();
}

MeshPrimitive::MeshPrimitive(std::shared_ptr<const Mesh> mesh, NormalMode normalMode)
    : m_mesh(std::move(mesh)),
      m_normalMode(normalMode) {
  buildLeaves();
}

void MeshPrimitive::buildLeaves() {
  if (!m_mesh)
    return;

  for (const auto& triangle : *m_mesh) {
    std::shared_ptr<Primitive> primitive;
    if (m_normalMode == NormalMode::Flat) {
      primitive =
        std::make_shared<FlatMeshTriangle>(m_mesh.get(), triangle[0], triangle[1], triangle[2]);
    } else {
      primitive =
        std::make_shared<SmoothMeshTriangle>(m_mesh.get(), triangle[0], triangle[1], triangle[2]);
    }
    m_leaves.push_back(std::move(primitive));
  }
}

const Primitive* MeshPrimitive::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                          render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return nullptr;
  }

  const Primitive* hit = nullptr;
  double minDistance = std::numeric_limits<double>::infinity();

  for (const auto& leaf : m_leaves) {
    HitPointInterval candidate;
    const Primitive* primitive = leaf->intersect(ray, candidate, state);
    if (primitive) {
      hitPoints = hitPoints + candidate;
      const double distance = candidate.minWithPositiveDistance().distance();
      if (distance < minDistance) {
        hit = primitive;
        minDistance = distance;
      }
    }
  }

  return hit && Primitive::material() ? this : hit;
}

bool MeshPrimitive::intersects(const Rayd& ray, render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return false;
  }

  for (const auto& leaf : m_leaves) {
    if (leaf->intersects(ray, state))
      return true;
  }

  return false;
}

RayPacketIntersection4 MeshPrimitive::intersectPacket(const Ray4& rays,
                                                      render::State& state) const {
  RayPacketIntersection4 result;
  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    HitPointInterval hitPoints;
    if (intersect(rays.rayd(lane), hitPoints, state)) {
      const auto& hit = hitPoints.minWithPositiveDistance();
      result.setHit(lane, static_cast<float>(hit.distance()), static_cast<float>(hit.distance()));
    }
  }
  return result;
}

void MeshPrimitive::forEachLeaf(std::shared_ptr<render::Material> inheritedMaterial,
                                const LeafVisitor& visitor) const {
  auto own = material();
  auto effective = own ? own : inheritedMaterial;

  for (const auto& leaf : m_leaves) {
    leaf->forEachLeaf(effective, visitor);
  }
}

void MeshPrimitive::forEachLeafInBounds(const BoundsFilter& boundsFilter,
                                        std::shared_ptr<render::Material> inheritedMaterial,
                                        const LeafVisitor& visitor) const {
  if (!boundsFilter(boundingBox())) {
    return;
  }

  auto own = material();
  auto effective = own ? own : inheritedMaterial;

  for (const auto& leaf : m_leaves) {
    leaf->forEachLeafInBounds(boundsFilter, effective, visitor);
  }
}

std::shared_ptr<Mesh> MeshPrimitive::tessellate(int lod) const {
  auto result = std::make_shared<Mesh>();
  int vertexOffset = 0;

  for (const auto& leaf : m_leaves) {
    auto childMesh = leaf->tessellate(lod);
    if (!childMesh)
      continue;

    for (const auto& v : childMesh->vertices())
      result->addVertex(v.point, v.normal, v.uv);

    for (std::size_t faceIndex = 0; faceIndex != childMesh->faces().size(); ++faceIndex) {
      const auto& face = childMesh->faces()[faceIndex];
      Mesh::Face remapped;
      remapped.reserve(face.size());
      for (int idx : face)
        remapped.push_back(idx + vertexOffset);
      const auto color = childMesh->faceColor(faceIndex);
      if (color)
        result->addFace(remapped, *color);
      else
        result->addFace(remapped);
    }

    vertexOffset += static_cast<int>(childMesh->vertices().size());
  }

  return result;
}

BoundingBoxd MeshPrimitive::calculateBoundingBox() const {
  BoundingBoxd bbox;
  for (const auto& leaf : m_leaves) {
    bbox.include(leaf->boundingBox());
  }
  return bbox;
}
