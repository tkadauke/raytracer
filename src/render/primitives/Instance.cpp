#include "render/State.h"
#include "render/primitives/Instance.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"
#include "core/geometry/Mesh.h"

#include <array>
#include <vector>

using namespace std;
using namespace render;

const Primitive* Instance::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                     render::State& state) const {
  // Static fast path — no motion blur math when velocity is zero.
  // Most instances in any given scene fall through here, so the
  // branch keeps the cost of the new feature to one comparison per
  // ray for unanimated geometry.
  if (m_velocity == Vector3d::null) {
    const Primitive* result = m_primitive->intersect(instancedRay(ray), hitPoints, state);
    if (result) {
      hitPoints = hitPoints.transform(m_pointMatrix, m_normalMatrix);
      if (Primitive::material()) {
        return this;
      } else {
        return result;
      }
    }
    return nullptr;
  }

  // Motion-blur path — the instance has translated by `velocity *
  // timeSample` since t=0. Equivalently, the ray has translated by
  // the negative of that in the instance's frame: shift the ray's
  // world origin by `-velocity * timeSample` before transforming
  // into local space (direction is unaffected by translation), then
  // build a `pointMatrix_at_t = pointMatrix + translate(velocity *
  // timeSample)` to map the resulting hit points back to world.
  Vector3d shift = m_velocity * state.timeSample;
  Rayd localRay(Vector4d(m_originMatrix.transformPoint(Vector3d(ray.origin()) - shift)),
                m_directionMatrix * ray.direction());

  const Primitive* result = m_primitive->intersect(localRay, hitPoints, state);
  if (result) {
    Matrix4d pointMatrixAtTime = m_pointMatrix;
    pointMatrixAtTime.setCell(0, 3, m_pointMatrix.cell(0, 3) + shift.x());
    pointMatrixAtTime.setCell(1, 3, m_pointMatrix.cell(1, 3) + shift.y());
    pointMatrixAtTime.setCell(2, 3, m_pointMatrix.cell(2, 3) + shift.z());
    hitPoints = hitPoints.transform(pointMatrixAtTime, m_normalMatrix);
    if (Primitive::material()) {
      return this;
    } else {
      return result;
    }
  }
  return nullptr;
}

PrimitivePacketHit4 Instance::intersectPacketHits(const Ray4& rays,
                                                  const PrimitivePacketState4& states) const {
  if (m_velocity != Vector3d::null) {
    return Primitive::intersectPacketHits(rays, states);
  }

  std::array<Rayd, Ray4::lanes> localRays{Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                          Rayd::undefined};
  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    localRays[lane] = instancedRay(rays.rayd(lane));
  }

  const PrimitivePacketHit4 childHits = m_primitive->intersectPacketHits(Ray4(localRays), states);
  PrimitivePacketHit4 result;
  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    if (!childHits.hit(lane)) {
      continue;
    }

    result.setHit(lane, Primitive::material() ? this : childHits.primitive(lane),
                  childHits.hitPoint(lane).transform(m_pointMatrix, m_normalMatrix),
                  childHits.scalarFallback(lane));
  }
  return result;
}

PrimitivePacketHit8 Instance::intersectPacketHits(const Ray8& rays,
                                                  const PrimitivePacketState8& states) const {
  if (m_velocity != Vector3d::null) {
    return Primitive::intersectPacketHits(rays, states);
  }

  std::array<Rayd, Ray8::lanes> localRays{Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                          Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                          Rayd::undefined, Rayd::undefined};
  for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
    localRays[lane] = instancedRay(rays.rayd(lane));
  }

  const PrimitivePacketHit8 childHits = m_primitive->intersectPacketHits(Ray8(localRays), states);
  PrimitivePacketHit8 result;
  for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
    if (!childHits.hit(lane)) {
      continue;
    }

    result.setHit(lane, Primitive::material() ? this : childHits.primitive(lane),
                  childHits.hitPoint(lane).transform(m_pointMatrix, m_normalMatrix),
                  childHits.scalarFallback(lane));
  }
  return result;
}

bool Instance::intersects(const Rayd& ray, render::State& state) const {
  if (m_velocity == Vector3d::null) {
    return m_primitive->intersects(instancedRay(ray), state);
  }
  Vector3d shift = m_velocity * state.timeSample;
  Rayd localRay(Vector4d(m_originMatrix.transformPoint(Vector3d(ray.origin()) - shift)),
                m_directionMatrix * ray.direction());
  return m_primitive->intersects(localRay, state);
}

void Instance::setMatrix(const Matrix4d& matrix) {
  m_pointMatrix = matrix;
  m_originMatrix = matrix.inverted();
  m_directionMatrix = Matrix3d(m_originMatrix);
  m_normalMatrix = m_directionMatrix.transposed();
}

void Instance::setVelocity(const Vector3d& velocity) {
  m_velocity = velocity;
}

std::shared_ptr<render::Material> Instance::material() const {
  auto parent = Primitive::material();
  if (parent)
    return parent;
  else
    return m_primitive->material();
}

BoundingBoxd Instance::calculateBoundingBox() const {
  BoundingBoxd original = m_primitive->boundingBox();
  vector<Vector3d> vertices;
  original.getVertices(vertices);

  BoundingBoxd result;
  for (const auto& vertex : vertices) {
    result.include(m_pointMatrix.transformPoint(vertex));
  }

  // For animated instances, the bbox must cover every position the
  // primitive occupies during the shutter — include the
  // end-of-shutter position too. Without this, a bbox-based
  // accelerator (Grid) could miss rays whose timeSample puts them
  // outside the static bbox.
  if (m_velocity != Vector3d::null) {
    BoundingBoxd shifted;
    for (const auto& vertex : vertices) {
      shifted.include(m_pointMatrix.transformPoint(vertex) + m_velocity);
    }
    result.include(shifted);
  }

  return result;
}

Vector3d Instance::farthestPoint(const Vector3d& direction) const {
  return m_pointMatrix.transformPoint(m_primitive->farthestPoint(m_directionMatrix * direction));
}

void Instance::forEachCurveOverlaySegment(const CurveOverlaySegmentVisitor& visitor) const {
  m_primitive->forEachCurveOverlaySegment(
    [&](const Vector3d& start, const Vector3d& end, const std::optional<Colord>& color) {
      visitor(m_pointMatrix.transformPoint(start), m_pointMatrix.transformPoint(end), color);
    });
}

void Instance::forEachTransformedLeaf(std::shared_ptr<render::Material> inheritedMaterial,
                                      const Matrix4d& pointMatrix, const Matrix3d& normalMatrix,
                                      const TransformedLeafVisitor& visitor) const {
  if (!m_primitive) {
    return;
  }

  auto own = Primitive::material();
  const auto composedPointMatrix = pointMatrix * m_pointMatrix;
  const auto composedNormalMatrix = normalMatrix * m_normalMatrix;
  if (!own) {
    m_primitive->forEachTransformedLeaf(inheritedMaterial, composedPointMatrix,
                                        composedNormalMatrix, visitor);
    return;
  }

  m_primitive->forEachTransformedLeaf(
    own, composedPointMatrix, composedNormalMatrix, [&](const TransformedLeaf& leaf) {
      visitor({leaf.primitive, own, leaf.pointMatrix, leaf.normalMatrix});
    });
}

void Instance::forEachTransformedLeafInBounds(const BoundsFilter& boundsFilter,
                                              std::shared_ptr<render::Material> inheritedMaterial,
                                              const Matrix4d& pointMatrix,
                                              const Matrix3d& normalMatrix,
                                              const TransformedLeafVisitor& visitor) const {
  if (!m_primitive) {
    return;
  }

  auto own = Primitive::material();
  TransformedLeaf instanceLeaf{this, own ? own : inheritedMaterial, pointMatrix, normalMatrix};
  if (!boundsFilter(instanceLeaf.boundingBox())) {
    return;
  }

  const auto composedPointMatrix = pointMatrix * m_pointMatrix;
  const auto composedNormalMatrix = normalMatrix * m_normalMatrix;
  if (!own) {
    m_primitive->forEachTransformedLeafInBounds(boundsFilter, inheritedMaterial,
                                                composedPointMatrix, composedNormalMatrix, visitor);
    return;
  }

  m_primitive->forEachTransformedLeafInBounds(
    boundsFilter, own, composedPointMatrix, composedNormalMatrix, [&](const TransformedLeaf& leaf) {
      visitor({leaf.primitive, own, leaf.pointMatrix, leaf.normalMatrix});
    });
}

std::shared_ptr<Mesh> Instance::tessellate(int lod) const {
  // Only the t=0 configuration is captured; a time-aware engine would need
  // per-frame meshes to handle motion blur correctly.
  auto childMesh = m_primitive->tessellate(lod);
  auto result = std::make_shared<Mesh>();
  if (!childMesh)
    return result;

  for (const auto& v : childMesh->vertices()) {
    Vector3d point = m_pointMatrix.transformPoint(v.point);
    Vector3d normal = (m_normalMatrix * v.normal).normalized();
    result->addVertex(point, normal, v.uv);
  }

  for (std::size_t i = 0; i != childMesh->faces().size(); ++i) {
    const auto color = childMesh->faceColor(i);
    const auto metadata = childMesh->faceMetadata(i);
    if (color)
      result->addFace(childMesh->faces()[i], *color, metadata);
    else
      result->addFace(childMesh->faces()[i], metadata);
  }

  return result;
}
