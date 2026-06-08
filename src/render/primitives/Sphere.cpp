#include "render/State.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/Stats.h"
#include "render/primitives/Sphere.h"
#include "core/SimdFeatures.h"
#include "core/geometry/Mesh.h"
#include "core/math/Constants.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include "core/simd/Float4.h"
#include <cmath>

using namespace std;
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
#if !(RAYTRACER_SIMD_SSE || RAYTRACER_SIMD_NEON)
  return Primitive::intersectPacket(rays, state);
#else
  using namespace core::simd;

  RAYTRACER_STATS_INC(raySphereIntersect);
  RayPacketIntersection4 result;

  const Float4 ox = load4(rays.originX.data()) - set1(static_cast<float>(m_origin.x()));
  const Float4 oy = load4(rays.originY.data()) - set1(static_cast<float>(m_origin.y()));
  const Float4 oz = load4(rays.originZ.data()) - set1(static_cast<float>(m_origin.z()));
  const Float4 dx = load4(rays.directionX.data());
  const Float4 dy = load4(rays.directionY.data());
  const Float4 dz = load4(rays.directionZ.data());

  const Float4 od = ox * dx + oy * dy + oz * dz;
  const Float4 dd = dx * dx + dy * dy + dz * dz;
  const Float4 oo = ox * ox + oy * oy + oz * oz;
  const Float4 radius2 = set1(static_cast<float>(m_radius * m_radius));
  const Float4 discriminant = od * od - dd * (oo - radius2);
  const Float4 zeroValue = zero();
  const Mask4 positiveDiscriminant = cmpGt(discriminant, zeroValue);
  const Float4 root = sqrt(discriminant);
  const Float4 negOd = zeroValue - od;
  const Float4 t1 = (negOd - root) / dd;
  const Float4 t2 = (negOd + root) / dd;
  const Mask4 positiveT = maskOr(cmpGt(t1, zeroValue), cmpGt(t2, zeroValue));
  const int hitMask = movemask(maskAnd(positiveDiscriminant, positiveT));
  const int discriminantMask = movemask(positiveDiscriminant);

  alignas(16) float nearDistances[4];
  alignas(16) float farDistances[4];
  store4(nearDistances, t1);
  store4(farDistances, t2);

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

template<typename Packet, typename StateArray, typename Result>
Result Sphere::intersectPacketHitsFor(const Packet& rays, const StateArray& states) const {
  Result result;
  for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
    if (!states[lane]) {
      continue;
    }
    State& state = *states[lane];
    RAYTRACER_STATS_INC(raySphereIntersect);
    const Rayd ray = rays.rayd(lane);
    const Vector3d o = ray.origin() - m_origin;
    const Vector3d d = ray.direction();

    const double od = o * d;
    const double dd = d * d;
    const double discriminant = od * od - dd * (o * o - m_radius * m_radius);

    if (discriminant <= 0.0) {
      state.miss(this, "Sphere, ray miss");
      continue;
    }

    const double discriminantRoot = sqrt(discriminant);
    const double t1 = (-od - discriminantRoot) / dd;
    const double t2 = (-od + discriminantRoot) / dd;
    if (t1 <= 0.0 && t2 <= 0.0) {
      state.miss(this, "Sphere, behind ray");
      continue;
    }

    const double t = t1 > 0.0 ? t1 : t2;
    const Vector3d hitPoint = ray.at(t);
    result.setHit(lane, this, HitPoint(this, t, hitPoint, (hitPoint - m_origin) / m_radius));
    state.hit(this, "Sphere");
  }
  return result;
}

PrimitivePacketHit4 Sphere::intersectPacketHits(const Ray4& rays,
                                                const PrimitivePacketState4& states) const {
  return intersectPacketHitsFor<Ray4, PrimitivePacketState4, PrimitivePacketHit4>(rays, states);
}

PrimitivePacketHit8 Sphere::intersectPacketHits(const Ray8& rays,
                                                const PrimitivePacketState8& states) const {
  return intersectPacketHitsFor<Ray8, PrimitivePacketState8, PrimitivePacketHit8>(rays, states);
}

template<typename Packet, typename StateArray, typename Result>
Result Sphere::intersectPacketIntervalsFor(const Packet& rays, const StateArray& states) const {
  Result result;
  for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
    if (!states[lane]) {
      continue;
    }
    State& state = *states[lane];
    RAYTRACER_STATS_INC(raySphereIntersect);
    const Rayd ray = rays.rayd(lane);
    const Vector3d o = ray.origin() - m_origin;
    const Vector3d d = ray.direction();

    const double od = o * d;
    const double dd = d * d;
    const double discriminant = od * od - dd * (o * o - m_radius * m_radius);

    if (discriminant <= 0.0) {
      state.miss(this, "Sphere, ray miss");
      continue;
    }

    const double discriminantRoot = sqrt(discriminant);
    const double t1 = (-od - discriminantRoot) / dd;
    const double t2 = (-od + discriminantRoot) / dd;
    const Vector3d hitPoint1 = ray.at(t1);
    const Vector3d hitPoint2 = ray.at(t2);
    HitPointInterval hitPoints(HitPoint(this, t1, hitPoint1, (hitPoint1 - m_origin) / m_radius),
                               HitPoint(this, t2, hitPoint2, (hitPoint2 - m_origin) / m_radius));

    if (t1 <= 0.0 && t2 <= 0.0) {
      result.setInterval(lane, nullptr, hitPoints);
      state.miss(this, "Sphere, behind ray");
      continue;
    }

    result.setInterval(lane, this, hitPoints);
    state.hit(this, "Sphere");
  }
  return result;
}

PrimitivePacketInterval4
Sphere::intersectPacketIntervals(const Ray4& rays, const PrimitivePacketState4& states) const {
  return intersectPacketIntervalsFor<Ray4, PrimitivePacketState4, PrimitivePacketInterval4>(rays,
                                                                                            states);
}

PrimitivePacketInterval8
Sphere::intersectPacketIntervals(const Ray8& rays, const PrimitivePacketState8& states) const {
  return intersectPacketIntervalsFor<Ray8, PrimitivePacketState8, PrimitivePacketInterval8>(rays,
                                                                                            states);
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

void Sphere::appendIntersectionSceneRecord(IntersectionSceneBuilder& builder,
                                           const TransformedLeaf& leaf) const {
  builder.addSphere(leaf, m_origin, m_radius);
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
