#include "render/State.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/primitives/Triangle.h"
#include "PacketStateHelpers.h"
#include "core/SimdFeatures.h"
#include "core/geometry/Mesh.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include "core/simd/Float4.h"

using namespace render;


const Primitive* Triangle::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                     render::State& state) const {
  double a = m_point0.x() - m_point1.x(), b = m_point0.x() - m_point2.x(), c = ray.direction().x(),
         d = m_point0.x() - ray.origin().x();
  double e = m_point0.y() - m_point1.y(), f = m_point0.y() - m_point2.y(), g = ray.direction().y(),
         h = m_point0.y() - ray.origin().y();
  double i = m_point0.z() - m_point1.z(), j = m_point0.z() - m_point2.z(), k = ray.direction().z(),
         l = m_point0.z() - ray.origin().z();

  double m = f * k - g * j, n = h * k - g * l, p = f * l - h * j;
  double q = g * i - e * k, r = e * l - h * i, s = e * j - f * i;

  double invDenom = 1.0 / (a * m + b * q + c * s);

  double e1 = d * m - b * n - c * p;
  double beta = e1 * invDenom;

  if (beta < 0.0 || beta > 1.0) {
    state.miss(this, "Triangle, beta not in [0, 1]");
    return nullptr;
  }

  double e2 = a * n + d * q + c * r;
  double gamma = e2 * invDenom;

  if (gamma < 0.0 || gamma > 1.0) {
    state.miss(this, "Triangle, gamma not in [0, 1]");
    return nullptr;
  }

  if (beta + gamma > 1.0) {
    state.miss(this, "Triangle, beta + gamma > 1");
    return nullptr;
  }

  double e3 = a * p - b * r + d * s;
  double t = e3 * invDenom;

  Vector3d hitPoint = ray.at(t);
  hitPoints.add(HitPoint(this, t, hitPoint, m_normal));

  if (t < 0) {
    state.miss(this, "Triangle, behind ray");
    return nullptr;
  } else {
    state.hit(this, "Triangle");
    return this;
  }
}

RayPacketIntersection4 Triangle::intersectPacket(const Ray4& rays, render::State& state) const {
#if !(RAYTRACER_SIMD_SSE || RAYTRACER_SIMD_NEON)
  return Primitive::intersectPacket(rays, state);
#else
  using namespace core::simd;

  RayPacketIntersection4 result;

  const Float4 a = set1(static_cast<float>(m_point0.x() - m_point1.x()));
  const Float4 b = set1(static_cast<float>(m_point0.x() - m_point2.x()));
  const Float4 d = set1(static_cast<float>(m_point0.x())) - load4(rays.originX.data());
  const Float4 e = set1(static_cast<float>(m_point0.y() - m_point1.y()));
  const Float4 f = set1(static_cast<float>(m_point0.y() - m_point2.y()));
  const Float4 h = set1(static_cast<float>(m_point0.y())) - load4(rays.originY.data());
  const Float4 i = set1(static_cast<float>(m_point0.z() - m_point1.z()));
  const Float4 j = set1(static_cast<float>(m_point0.z() - m_point2.z()));
  const Float4 l = set1(static_cast<float>(m_point0.z())) - load4(rays.originZ.data());
  const Float4 c = load4(rays.directionX.data());
  const Float4 g = load4(rays.directionY.data());
  const Float4 k = load4(rays.directionZ.data());

  const Float4 m = f * k - g * j;
  const Float4 n = h * k - g * l;
  const Float4 p = f * l - h * j;
  const Float4 q = g * i - e * k;
  const Float4 r = e * l - h * i;
  const Float4 s = e * j - f * i;
  const Float4 denom = a * m + b * q + c * s;
  const Float4 invDenom = set1(1.0f) / denom;

  const Float4 e1 = d * m - b * n - c * p;
  const Float4 beta = e1 * invDenom;
  const Float4 e2 = a * n + d * q + c * r;
  const Float4 gamma = e2 * invDenom;
  const Float4 e3 = a * p - b * r + d * s;
  const Float4 t = e3 * invDenom;

  const Float4 zeroValue = zero();
  const Float4 one = set1(1.0f);
  Mask4 hit = maskAnd(cmpGe(beta, zeroValue), cmpLe(beta, one));
  hit = maskAnd(hit, cmpGe(gamma, zeroValue));
  hit = maskAnd(hit, cmpLe(gamma, one));
  hit = maskAnd(hit, cmpLe(beta + gamma, one));
  hit = maskAnd(hit, cmpGe(t, zeroValue));

  alignas(16) float distances[4];
  store4(distances, t);
  const int hitMask = movemask(hit);
  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    if ((hitMask & (1 << lane)) != 0) {
      result.setHit(lane, distances[lane], distances[lane]);
      packetHit(state, this, "Triangle");
    } else {
      packetMiss(state, this, "Triangle, ray miss");
    }
  }

  return result;
#endif
}

template<typename Packet, typename StateArray, typename Result>
Result Triangle::intersectPacketHitsFor(const Packet& rays, const StateArray& states) const {
  Result result;
  for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
    if (!states[lane]) {
      continue;
    }
    State& state = *states[lane];
    const Rayd ray = rays.rayd(lane);

    const double a = m_point0.x() - m_point1.x();
    const double b = m_point0.x() - m_point2.x();
    const double c = ray.direction().x();
    const double d = m_point0.x() - ray.origin().x();
    const double e = m_point0.y() - m_point1.y();
    const double f = m_point0.y() - m_point2.y();
    const double g = ray.direction().y();
    const double h = m_point0.y() - ray.origin().y();
    const double i = m_point0.z() - m_point1.z();
    const double j = m_point0.z() - m_point2.z();
    const double k = ray.direction().z();
    const double l = m_point0.z() - ray.origin().z();

    const double m = f * k - g * j;
    const double n = h * k - g * l;
    const double p = f * l - h * j;
    const double q = g * i - e * k;
    const double r = e * l - h * i;
    const double s = e * j - f * i;

    const double invDenom = 1.0 / (a * m + b * q + c * s);
    const double beta = (d * m - b * n - c * p) * invDenom;
    if (beta < 0.0 || beta > 1.0) {
      state.miss(this, "Triangle, beta not in [0, 1]");
      continue;
    }

    const double gamma = (a * n + d * q + c * r) * invDenom;
    if (gamma < 0.0 || gamma > 1.0) {
      state.miss(this, "Triangle, gamma not in [0, 1]");
      continue;
    }

    if (beta + gamma > 1.0) {
      state.miss(this, "Triangle, beta + gamma > 1");
      continue;
    }

    const double t = (a * p - b * r + d * s) * invDenom;
    if (t < 0.0) {
      state.miss(this, "Triangle, behind ray");
      continue;
    }

    state.hit(this, "Triangle");
    if (t > 0.0) {
      result.setHit(lane, this, HitPoint(this, t, ray.at(t), m_normal));
    }
  }
  return result;
}

PrimitivePacketHit4 Triangle::intersectPacketHits(const Ray4& rays,
                                                  const PrimitivePacketState4& states) const {
  return intersectPacketHitsFor<Ray4, PrimitivePacketState4, PrimitivePacketHit4>(rays, states);
}

PrimitivePacketHit8 Triangle::intersectPacketHits(const Ray8& rays,
                                                  const PrimitivePacketState8& states) const {
  return intersectPacketHitsFor<Ray8, PrimitivePacketState8, PrimitivePacketHit8>(rays, states);
}

void Triangle::appendIntersectionSceneRecord(IntersectionSceneBuilder& builder,
                                             const TransformedLeaf& leaf) const {
  builder.addTriangle(leaf, IntersectionTrianglePayload{m_point0, m_point1, m_point2, m_normal,
                                                        m_normal, m_normal, Vector2d(0, 0),
                                                        Vector2d(1, 0), Vector2d(0, 1)});
}

std::shared_ptr<Mesh> Triangle::tessellate(int) const {
  auto mesh = std::make_shared<Mesh>();
  mesh->addVertex(m_point0, m_normal, Vector2d(0, 0));
  mesh->addVertex(m_point1, m_normal, Vector2d(1, 0));
  mesh->addVertex(m_point2, m_normal, Vector2d(0, 1));
  mesh->addFace({0, 1, 2});
  return mesh;
}

Vector3d Triangle::computeNormal() const {
  Vector3d normal = (m_point1 - m_point0) ^ (m_point2 - m_point0);
  return normal.normalized();
}

BoundingBoxd Triangle::calculateBoundingBox() const {
  BoundingBoxd b;
  b.include(m_point0);
  b.include(m_point1);
  b.include(m_point2);
  return b;
}
