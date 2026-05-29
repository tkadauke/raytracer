#include "render/State.h"
#include "render/primitives/Plane.h"
#include "core/SimdFeatures.h"
#include "core/geometry/Mesh.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include "core/simd/Float4.h"
#include <QDebug>

using namespace render;

#if RAYTRACER_SIMD_SSE || RAYTRACER_SIMD_NEON
namespace {
  void packetHit(State& state, const Primitive* primitive, const std::string& reason) {
    if (state.traceEvents) {
      state.hit(primitive, reason);
    } else {
      ++state.intersectionHits;
    }
  }

  void packetMiss(State& state, const Primitive* primitive, const std::string& reason) {
    if (state.traceEvents) {
      state.miss(primitive, reason);
    } else {
      ++state.intersectionMisses;
    }
  }
}
#endif

const Primitive* Plane::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                  render::State& state) const {
  double t = calculateIntersectionDistance(ray);

  if (t > 0) {
    hitPoints.add(HitPoint(this, t, ray.at(t), m_normal));
    state.hit(this, "Plane");
    return this;
  } else {
    state.miss(this, "Plane, ray miss");
    return nullptr;
  }
}

RayPacketIntersection4 Plane::intersectPacket(const Ray4& rays, render::State& state) const {
#if !(RAYTRACER_SIMD_SSE || RAYTRACER_SIMD_NEON)
  return Primitive::intersectPacket(rays, state);
#else
  using namespace core::simd;

  RayPacketIntersection4 result;

  const Float4 nx = set1(static_cast<float>(m_normal.x()));
  const Float4 ny = set1(static_cast<float>(m_normal.y()));
  const Float4 nz = set1(static_cast<float>(m_normal.z()));
  const Float4 distance = set1(static_cast<float>(m_distance));
  const Float4 ox = load4(rays.originX.data());
  const Float4 oy = load4(rays.originY.data());
  const Float4 oz = load4(rays.originZ.data());
  const Float4 dx = load4(rays.directionX.data());
  const Float4 dy = load4(rays.directionY.data());
  const Float4 dz = load4(rays.directionZ.data());
  const Float4 angle = nx * dx + ny * dy + nz * dz;
  const Float4 zeroValue = zero();
  const Float4 numerator = zeroValue - (nx * ox + ny * oy + nz * oz + distance);
  const Float4 t = numerator / angle;
  const Mask4 hit = maskAnd(cmpNe(angle, zeroValue), cmpGt(t, zeroValue));

  alignas(16) float distances[4];
  store4(distances, t);
  const int hitMask = movemask(hit);
  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    if ((hitMask & (1 << lane)) != 0) {
      result.setHit(lane, distances[lane], distances[lane]);
      packetHit(state, this, "Plane");
    } else {
      packetMiss(state, this, "Plane, ray miss");
    }
  }

  return result;
#endif
}

bool Plane::intersects(const Rayd& ray, render::State& state) const {
  if (calculateIntersectionDistance(ray) > 0) {
    state.shadowHit(this, "Plane");
    return true;
  }

  state.shadowMiss(this, "Plane");
  return false;
}

double Plane::calculateIntersectionDistance(const Rayd& ray) const {
  const Vector3d &o = ray.origin(), d = ray.direction();

  double angle = m_normal * d;
  if (angle == 0)
    return false;

  return -(m_normal * o + m_distance) / angle;
}

std::shared_ptr<Mesh> Plane::tessellate(int) const {
  qWarning()
    << "Plane is infinite; tessellate() returns empty mesh — clip to a finite region first.";
  return std::make_shared<Mesh>();
}

BoundingBoxd Plane::calculateBoundingBox() const {
  return BoundingBoxd::infinity;
}
