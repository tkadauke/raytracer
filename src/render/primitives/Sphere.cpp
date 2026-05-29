#include "render/State.h"
#include "render/Stats.h"
#include "render/primitives/Sphere.h"
#include "core/SimdFeatures.h"
#include "core/geometry/Mesh.h"
#include "core/math/Constants.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include <cmath>
#if RAYTRACER_SIMD_SSE
#include <xmmintrin.h>
#endif

using namespace std;
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

const Primitive* Sphere::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                   render::State& state) const {
  RAYTRACER_STATS_INC(raySphereIntersect);
  const Vector3d &o = ray.origin() - m_origin, d = ray.direction();

  double od = o * d, dd = d * d;
  double discriminant = od * od - dd * (o * o - m_radius * m_radius);

  if (discriminant < 0) {
    state.miss(this, "Sphere, ray miss");
    return nullptr;
  } else if (discriminant > 0) {
    double discriminantRoot = sqrt(discriminant);
    double t1 = (-od - discriminantRoot) / dd;
    double t2 = (-od + discriminantRoot) / dd;

    Vector3d hitPoint1 = ray.at(t1), hitPoint2 = ray.at(t2);

    hitPoints.add(HitPoint(this, t1, hitPoint1, (hitPoint1 - m_origin) / m_radius),
                  HitPoint(this, t2, hitPoint2, (hitPoint2 - m_origin) / m_radius));

    if (t1 <= 0 && t2 <= 0) {
      state.miss(this, "Sphere, behind ray");
      return nullptr;
    } else {
      state.hit(this, "Sphere");
      return this;
    }
  }
  state.miss(this, "Sphere, ray miss");
  return nullptr;
}

RayPacketIntersection4 Sphere::intersectPacket(const Ray4& rays, render::State& state) const {
#if !RAYTRACER_SIMD_SSE
  return Primitive::intersectPacket(rays, state);
#else
  RAYTRACER_STATS_INC(raySphereIntersect);
  RayPacketIntersection4 result;

  const __m128 ox =
    _mm_sub_ps(_mm_load_ps(rays.originX.data()), _mm_set1_ps(static_cast<float>(m_origin.x())));
  const __m128 oy =
    _mm_sub_ps(_mm_load_ps(rays.originY.data()), _mm_set1_ps(static_cast<float>(m_origin.y())));
  const __m128 oz =
    _mm_sub_ps(_mm_load_ps(rays.originZ.data()), _mm_set1_ps(static_cast<float>(m_origin.z())));
  const __m128 dx = _mm_load_ps(rays.directionX.data());
  const __m128 dy = _mm_load_ps(rays.directionY.data());
  const __m128 dz = _mm_load_ps(rays.directionZ.data());

  const __m128 od =
    _mm_add_ps(_mm_add_ps(_mm_mul_ps(ox, dx), _mm_mul_ps(oy, dy)), _mm_mul_ps(oz, dz));
  const __m128 dd =
    _mm_add_ps(_mm_add_ps(_mm_mul_ps(dx, dx), _mm_mul_ps(dy, dy)), _mm_mul_ps(dz, dz));
  const __m128 oo =
    _mm_add_ps(_mm_add_ps(_mm_mul_ps(ox, ox), _mm_mul_ps(oy, oy)), _mm_mul_ps(oz, oz));
  const __m128 radius2 = _mm_set1_ps(static_cast<float>(m_radius * m_radius));
  const __m128 discriminant =
    _mm_sub_ps(_mm_mul_ps(od, od), _mm_mul_ps(dd, _mm_sub_ps(oo, radius2)));
  const __m128 zero = _mm_setzero_ps();
  const __m128 positiveDiscriminant = _mm_cmpgt_ps(discriminant, zero);
  const __m128 root = _mm_sqrt_ps(discriminant);
  const __m128 negOd = _mm_sub_ps(zero, od);
  const __m128 t1 = _mm_div_ps(_mm_sub_ps(negOd, root), dd);
  const __m128 t2 = _mm_div_ps(_mm_add_ps(negOd, root), dd);
  const __m128 positiveT = _mm_or_ps(_mm_cmpgt_ps(t1, zero), _mm_cmpgt_ps(t2, zero));
  const int hitMask = _mm_movemask_ps(_mm_and_ps(positiveDiscriminant, positiveT));
  const int discriminantMask = _mm_movemask_ps(positiveDiscriminant);

  alignas(16) float nearDistances[4];
  alignas(16) float farDistances[4];
  _mm_store_ps(nearDistances, t1);
  _mm_store_ps(farDistances, t2);

  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    const int laneBit = 1 << lane;
    if ((hitMask & laneBit) != 0) {
      result.setHit(lane, nearDistances[lane], farDistances[lane]);
      packetHit(state, this, "Sphere");
    } else if ((discriminantMask & laneBit) != 0) {
      packetMiss(state, this, "Sphere, behind ray");
    } else {
      packetMiss(state, this, "Sphere, ray miss");
    }
  }

  return result;
#endif
}

bool Sphere::intersects(const Rayd& ray, render::State& state) const {
  RAYTRACER_STATS_INC(raySphereIntersects);
  const Vector3d &o = ray.origin() - m_origin, d = ray.direction();

  double od = o * d, dd = d * d;
  double discriminant = od * od - dd * (o * o - m_radius * m_radius);

  if (discriminant < 0) {
    state.shadowMiss(this, "Sphere, ray miss");
    return false;
  } else if (discriminant > 0) {
    double discriminantRoot = sqrt(discriminant);
    double t1 = (-od - discriminantRoot) / dd;
    double t2 = (-od + discriminantRoot) / dd;
    if (t1 <= 0 && t2 <= 0) {
      state.shadowMiss(this, "Sphere, behind ray");
      return false;
    }

    state.shadowHit(this, "Sphere");
    return true;
  }

  state.shadowMiss(this, "Sphere, ray miss");
  return false;
}

shared_ptr<Mesh> Sphere::tessellate(int lod) const {
  const int lonSegs = 16 << lod; // 16, 32, 64, …
  const int latBands = 8 << lod; // 8, 16, 32, …

  auto mesh = make_shared<Mesh>();

  // Build a (latBands+1) × (lonSegs+1) vertex grid. Polar rows share the
  // same 3D point but carry distinct u-values — avoids a UV pinch at the poles
  // without needing special-case fan triangles.
  for (int lat = 0; lat <= latBands; ++lat) {
    double theta = -PI / 2.0 + lat * PI / latBands; // [-π/2, π/2]
    double cosTheta = std::cos(theta);
    double sinTheta = std::sin(theta);
    double v = static_cast<double>(lat) / latBands; // [0, 1] south→north

    for (int lon = 0; lon <= lonSegs; ++lon) {
      double phi = lon * TAU / lonSegs; // [0, 2π]
      double cosPhi = std::cos(phi);
      double sinPhi = std::sin(phi);
      double u = static_cast<double>(lon) / lonSegs; // [0, 1] around

      Vector3d normal(cosTheta * cosPhi, sinTheta, cosTheta * sinPhi);
      Vector3d point = m_origin + normal * m_radius;
      mesh->addVertex(point, normal, Vector2d(u, v));
    }
  }

  // One quad per (lat-band, lon-segment) cell — uniform topology, no fans.
  // Corners are listed CCW when viewed from outside so raster backface
  // culling agrees with the outward radial vertex normals.
  for (int lat = 0; lat < latBands; ++lat) {
    for (int lon = 0; lon < lonSegs; ++lon) {
      int row = lat * (lonSegs + 1);
      int nextRow = (lat + 1) * (lonSegs + 1);
      mesh->addFace({row + lon, nextRow + lon, nextRow + lon + 1, row + lon + 1});
    }
  }

  return mesh;
}

BoundingBoxd Sphere::calculateBoundingBox() const {
  Vector3d radius(m_radius, m_radius, m_radius);
  return BoundingBoxd(m_origin - radius, m_origin + radius);
}

Vector3d Sphere::farthestPoint(const Vector3d& direction) const {
  return m_origin + direction.normalized() * m_radius;
}
