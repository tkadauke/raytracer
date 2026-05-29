#include "render/State.h"
#include "render/primitives/Plane.h"
#include "core/SimdFeatures.h"
#include "core/geometry/Mesh.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include <QDebug>
#if RAYTRACER_SIMD_SSE
#include <xmmintrin.h>
#endif

using namespace render;

#if RAYTRACER_SIMD_SSE
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
#if !RAYTRACER_SIMD_SSE
  return Primitive::intersectPacket(rays, state);
#else
  RayPacketIntersection4 result;

  const __m128 nx = _mm_set1_ps(static_cast<float>(m_normal.x()));
  const __m128 ny = _mm_set1_ps(static_cast<float>(m_normal.y()));
  const __m128 nz = _mm_set1_ps(static_cast<float>(m_normal.z()));
  const __m128 distance = _mm_set1_ps(static_cast<float>(m_distance));
  const __m128 ox = _mm_load_ps(rays.originX.data());
  const __m128 oy = _mm_load_ps(rays.originY.data());
  const __m128 oz = _mm_load_ps(rays.originZ.data());
  const __m128 dx = _mm_load_ps(rays.directionX.data());
  const __m128 dy = _mm_load_ps(rays.directionY.data());
  const __m128 dz = _mm_load_ps(rays.directionZ.data());
  const __m128 angle =
    _mm_add_ps(_mm_add_ps(_mm_mul_ps(nx, dx), _mm_mul_ps(ny, dy)), _mm_mul_ps(nz, dz));
  const __m128 numerator = _mm_sub_ps(
    _mm_setzero_ps(),
    _mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(nx, ox), _mm_mul_ps(ny, oy)), _mm_mul_ps(nz, oz)),
               distance));
  const __m128 t = _mm_div_ps(numerator, angle);
  const __m128 hit =
    _mm_and_ps(_mm_cmpneq_ps(angle, _mm_setzero_ps()), _mm_cmpgt_ps(t, _mm_setzero_ps()));

  alignas(16) float distances[4];
  _mm_store_ps(distances, t);
  const int hitMask = _mm_movemask_ps(hit);
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
