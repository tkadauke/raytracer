#include "render/State.h"
#include "render/primitives/Composite.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/geometry/Mesh.h"

#include <array>
#include <limits>

using namespace std;
using namespace render;

template<typename Packet, typename Result>
Result Composite::intersectPacketImpl(const Packet& rays, render::State& state) const {
  Result result;
  std::array<float, Packet::lanes> nearest;
  nearest.fill(std::numeric_limits<float>::infinity());

  uint16_t activeMask = 0;
  for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
    if (boundingBox().intersects(rays.rayd(lane))) {
      activeMask |= static_cast<uint16_t>(1u << lane);
    }
  }
  if (!activeMask) {
    return result;
  }

  for (const auto& primitive : primitives()) {
    const Result candidate = primitive->intersectPacket(rays, state);
    for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
      if ((activeMask & (1u << lane)) && candidate.hit(lane) &&
          candidate.tNear[lane] < nearest[lane]) {
        nearest[lane] = candidate.tNear[lane];
        result.setHit(lane, candidate.tNear[lane], candidate.tFar[lane]);
      }
    }
  }

  return result;
}

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

RayPacketIntersection4 Composite::intersectPacket(const Ray4& rays, render::State& state) const {
  return intersectPacketImpl<Ray4, RayPacketIntersection4>(rays, state);
}

RayPacketIntersection8 Composite::intersectPacket(const Ray8& rays, render::State& state) const {
  return intersectPacketImpl<Ray8, RayPacketIntersection8>(rays, state);
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

void Composite::setup() {
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

void Composite::forEachTransformedLeaf(std::shared_ptr<render::Material> inheritedMaterial,
                                       const Matrix4d& pointMatrix, const Matrix3d& normalMatrix,
                                       const TransformedLeafVisitor& visitor) const {
  auto own = material();
  auto effective = own ? own : inheritedMaterial;

  for (const auto& primitive : m_primitives) {
    primitive->forEachTransformedLeaf(effective, pointMatrix, normalMatrix, visitor);
  }
}

void Composite::forEachTransformedLeafInBounds(const BoundsFilter& boundsFilter,
                                               std::shared_ptr<render::Material> inheritedMaterial,
                                               const Matrix4d& pointMatrix,
                                               const Matrix3d& normalMatrix,
                                               const TransformedLeafVisitor& visitor) const {
  TransformedLeaf group{this, inheritedMaterial, pointMatrix, normalMatrix};
  if (!boundsFilter(group.boundingBox())) {
    return;
  }

  auto own = material();
  auto effective = own ? own : inheritedMaterial;

  for (const auto& primitive : m_primitives) {
    primitive->forEachTransformedLeafInBounds(boundsFilter, effective, pointMatrix, normalMatrix,
                                              visitor);
  }
}

void Composite::forEachCurveOverlaySegment(const CurveOverlaySegmentVisitor& visitor) const {
  for (const auto& primitive : m_primitives) {
    primitive->forEachCurveOverlaySegment(visitor);
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

    for (std::size_t faceIndex = 0; faceIndex != childMesh->faces().size(); ++faceIndex) {
      const auto& face = childMesh->faces()[faceIndex];
      Mesh::Face remapped;
      remapped.reserve(face.size());
      for (int idx : face)
        remapped.push_back(idx + vertexOffset);
      const auto color = childMesh->faceColor(faceIndex);
      const auto metadata = childMesh->faceMetadata(faceIndex);
      if (color)
        result->addFace(remapped, *color, metadata);
      else
        result->addFace(remapped, metadata);
    }

    vertexOffset += static_cast<int>(childMesh->vertices().size());
  }

  return result;
}
