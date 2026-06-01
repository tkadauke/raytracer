#include "render/State.h"
#include "render/primitives/Rectangle.h"
#include "core/geometry/Mesh.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"

#include <cmath>

using namespace render;

const Primitive* Rectangle::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                      render::State& state) const {
  double t = (m_corner - ray.origin()) * m_normal / (ray.direction() * m_normal);
  if (std::isinf(t)) {
    state.miss(this, "Rectangle, parallel");
    return nullptr;
  }

  Vector3d hitPoint = ray.at(t);
  Vector3d difference = hitPoint - m_corner;

  double dot1 = difference * m_leg1;

  if (dot1 < 0 || dot1 > m_squaredLength1) {
    state.miss(this, "Rectangle, outside u axis");
    return nullptr;
  }

  double dot2 = difference * m_leg2;

  if (dot2 < 0 || dot2 > m_squaredLength2) {
    state.miss(this, "Rectangle, outside v axis");
    return nullptr;
  }

  if (-ray.direction() * m_normal < 0.0) {
    hitPoints.addOut(HitPoint(this, t, hitPoint, m_normal));
  } else {
    hitPoints.addIn(HitPoint(this, t, hitPoint, m_normal));
  }

  if (t < 0) {
    state.miss(this, "Rectangle, behind ray");
    return nullptr;
  }

  state.hit(this, "Rectangle");
  return this;
}

PrimitivePacketHit4 Rectangle::intersectPacketHits(const Ray4& rays,
                                                   const PrimitivePacketState4& states) const {
  PrimitivePacketHit4 result;
  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    State fallbackState;
    State& state = states[lane] ? *states[lane] : fallbackState;
    const Rayd ray = rays.rayd(lane);
    const double t = (m_corner - ray.origin()) * m_normal / (ray.direction() * m_normal);
    if (std::isinf(t)) {
      state.miss(this, "Rectangle, parallel");
      continue;
    }

    const Vector3d hitPoint = ray.at(t);
    const Vector3d difference = hitPoint - m_corner;
    const double dot1 = difference * m_leg1;
    if (dot1 < 0.0 || dot1 > m_squaredLength1) {
      state.miss(this, "Rectangle, outside u axis");
      continue;
    }

    const double dot2 = difference * m_leg2;
    if (dot2 < 0.0 || dot2 > m_squaredLength2) {
      state.miss(this, "Rectangle, outside v axis");
      continue;
    }

    if (t < 0.0) {
      state.miss(this, "Rectangle, behind ray");
      continue;
    }

    state.hit(this, "Rectangle");
    result.setHit(lane, this, HitPoint(this, t, hitPoint, m_normal));
  }
  return result;
}

std::shared_ptr<Mesh> Rectangle::tessellate(int) const {
  auto mesh = std::make_shared<Mesh>();

  Vector3d p0(m_corner.x(), m_corner.y(), m_corner.z());
  Vector3d p1 = p0 + m_leg1;
  Vector3d p2 = p0 + m_leg1 + m_leg2;
  Vector3d p3 = p0 + m_leg2;

  mesh->addVertex(p0, m_normal, Vector2d(0, 0));
  mesh->addVertex(p1, m_normal, Vector2d(1, 0));
  mesh->addVertex(p2, m_normal, Vector2d(1, 1));
  mesh->addVertex(p3, m_normal, Vector2d(0, 1));
  mesh->addFace({0, 1, 2});
  mesh->addFace({0, 2, 3});

  return mesh;
}

BoundingBoxd Rectangle::calculateBoundingBox() const {
  BoundingBoxd b;
  b.include(m_corner);
  b.include(m_corner + m_leg1);
  b.include(m_corner + m_leg2);
  b.include(m_corner + m_leg1 + m_leg2);
  return b.grownByEpsilon();
}
