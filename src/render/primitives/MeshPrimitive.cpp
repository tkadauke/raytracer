#include "render/primitives/MeshPrimitive.h"

#include "core/geometry/MeshAsset.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "render/State.h"
#include "render/primitives/FlatMeshTriangle.h"
#include "render/primitives/SmoothMeshTriangle.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

using namespace render;

MeshPrimitive::MeshPrimitive(Mesh mesh, NormalMode normalMode)
    : MeshPrimitive(std::move(mesh), FaceMaterials(), normalMode) {
}

MeshPrimitive::MeshPrimitive(Mesh mesh, FaceMaterials faceMaterials, NormalMode normalMode)
    : m_asset(std::make_shared<core::MeshAsset>(std::move(mesh))),
      m_mesh(m_asset->mesh()),
      m_faceMaterials(std::move(faceMaterials)),
      m_normalMode(normalMode) {
  buildLeaves();
}

MeshPrimitive::MeshPrimitive(std::shared_ptr<const Mesh> mesh, NormalMode normalMode)
    : MeshPrimitive(std::move(mesh), FaceMaterials(), normalMode) {
}

MeshPrimitive::MeshPrimitive(std::shared_ptr<const Mesh> mesh, FaceMaterials faceMaterials,
                             NormalMode normalMode)
    : m_mesh(std::move(mesh)),
      m_faceMaterials(std::move(faceMaterials)),
      m_normalMode(normalMode) {
  buildLeaves();
}

MeshPrimitive::MeshPrimitive(std::shared_ptr<const core::MeshAsset> asset, NormalMode normalMode)
    : MeshPrimitive(std::move(asset), FaceMaterials(), normalMode) {
}

MeshPrimitive::MeshPrimitive(std::shared_ptr<const core::MeshAsset> asset,
                             FaceMaterials faceMaterials, NormalMode normalMode)
    : m_asset(std::move(asset)),
      m_mesh(m_asset ? m_asset->mesh() : nullptr),
      m_faceMaterials(std::move(faceMaterials)),
      m_normalMode(normalMode) {
  buildLeaves();
}

void MeshPrimitive::buildLeaves() {
  if (!m_mesh)
    return;

  for (std::size_t faceIndex = 0; faceIndex != m_mesh->faces().size(); ++faceIndex) {
    const auto& face = m_mesh->faces()[faceIndex];
    for (std::size_t vertexIndex = 2; vertexIndex < face.size(); ++vertexIndex) {
      if (!isBuildableTriangle(face[0], face[vertexIndex - 1], face[vertexIndex]))
        continue;

      auto primitive = buildLeaf(faceIndex, face[0], face[vertexIndex - 1], face[vertexIndex]);
      if (auto material = materialForFace(faceIndex))
        primitive->setMaterial(std::move(material));
      m_leaves.push_back(std::move(primitive));
    }
  }
}

bool MeshPrimitive::isBuildableTriangle(int index0, int index1, int index2) const {
  if (!hasValidVertexIndex(index0) || !hasValidVertexIndex(index1) ||
      !hasValidVertexIndex(index2)) {
    return false;
  }

  const Vector3d& p0 = m_mesh->vertices()[index0].point;
  const Vector3d& p1 = m_mesh->vertices()[index1].point;
  const Vector3d& p2 = m_mesh->vertices()[index2].point;
  const Vector3d edge01 = p1 - p0;
  const Vector3d edge02 = p2 - p0;
  const Vector3d edge12 = p2 - p1;
  const double maxEdgeSquared =
    std::max({edge01.squaredLength(), edge02.squaredLength(), edge12.squaredLength()});
  if (maxEdgeSquared == 0.0)
    return false;

  constexpr double relativeAreaTolerance = 1.0e-24;
  return (edge01 ^ edge02).squaredLength() >
         maxEdgeSquared * maxEdgeSquared * relativeAreaTolerance;
}

bool MeshPrimitive::hasValidVertexIndex(int index) const {
  return m_mesh && index >= 0 && static_cast<std::size_t>(index) < m_mesh->vertices().size();
}

std::shared_ptr<Primitive> MeshPrimitive::buildLeaf(std::size_t faceIndex, int index0, int index1,
                                                    int index2) const {
  const auto metadata = m_mesh->faceMetadata(faceIndex);
  if (m_normalMode == NormalMode::Flat)
    return std::make_shared<FlatMeshTriangle>(m_mesh.get(), index0, index1, index2, metadata);

  return std::make_shared<SmoothMeshTriangle>(m_mesh.get(), index0, index1, index2, metadata);
}

std::shared_ptr<render::Material> MeshPrimitive::materialForFace(std::size_t faceIndex) const {
  if (faceIndex >= m_faceMaterials.size())
    return nullptr;

  return m_faceMaterials[faceIndex];
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

  return hit && Primitive::material() && !hit->material() ? this : hit;
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

PrimitivePacketHit4 MeshPrimitive::intersectPacketHits(const Ray4& rays,
                                                       const PrimitivePacketState4& states) const {
  PrimitivePacketHit4 result;
  std::array<bool, Ray4::lanes> activeLanes{};
  bool hasActiveLane = false;

  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    activeLanes[lane] = boundingBoxIntersects(rays.rayd(lane));
    hasActiveLane = hasActiveLane || activeLanes[lane];
  }

  if (!hasActiveLane) {
    return result;
  }

  for (const auto& leaf : m_leaves) {
    const PrimitivePacketHit4 candidate = leaf->intersectPacketHits(rays, states);
    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      if (activeLanes[lane] && candidate.hit(lane)) {
        result.setHitIfCloser(lane, candidate.primitive(lane), candidate.hitPoint(lane));
      }
    }
  }

  if (Primitive::material()) {
    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      if (result.hit(lane) && !result.primitive(lane)->material()) {
        result.setHit(lane, this, result.hitPoint(lane));
      }
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

void MeshPrimitive::forEachTransformedLeaf(std::shared_ptr<render::Material> inheritedMaterial,
                                           const Matrix4d& pointMatrix,
                                           const Matrix3d& normalMatrix,
                                           const TransformedLeafVisitor& visitor) const {
  auto own = material();
  auto effective = own ? own : inheritedMaterial;

  for (const auto& leaf : m_leaves) {
    leaf->forEachTransformedLeaf(effective, pointMatrix, normalMatrix, visitor);
  }
}

void MeshPrimitive::forEachTransformedLeafInBounds(
  const BoundsFilter& boundsFilter, std::shared_ptr<render::Material> inheritedMaterial,
  const Matrix4d& pointMatrix, const Matrix3d& normalMatrix,
  const TransformedLeafVisitor& visitor) const {
  TransformedLeaf group{this, inheritedMaterial, pointMatrix, normalMatrix};
  if (!boundsFilter(group.boundingBox())) {
    return;
  }

  auto own = material();
  auto effective = own ? own : inheritedMaterial;

  for (const auto& leaf : m_leaves) {
    leaf->forEachTransformedLeafInBounds(boundsFilter, effective, pointMatrix, normalMatrix,
                                         visitor);
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

BoundingBoxd MeshPrimitive::calculateBoundingBox() const {
  BoundingBoxd bbox;
  for (const auto& leaf : m_leaves) {
    bbox.include(leaf->boundingBox());
  }
  return bbox;
}
