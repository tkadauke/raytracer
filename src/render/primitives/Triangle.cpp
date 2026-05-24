#include "render/State.h"
#include "render/primitives/Triangle.h"
#include "core/geometry/Mesh.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#ifdef __SSE__
#include <xmmintrin.h>
#endif

using namespace render;

#ifdef __SSE__
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
#ifndef __SSE__
  return Primitive::intersectPacket(rays, state);
#else
  RayPacketIntersection4 result;

  const __m128 a = _mm_set1_ps(static_cast<float>(m_point0.x() - m_point1.x()));
  const __m128 b = _mm_set1_ps(static_cast<float>(m_point0.x() - m_point2.x()));
  const __m128 d =
    _mm_sub_ps(_mm_set1_ps(static_cast<float>(m_point0.x())), _mm_load_ps(rays.originX.data()));
  const __m128 e = _mm_set1_ps(static_cast<float>(m_point0.y() - m_point1.y()));
  const __m128 f = _mm_set1_ps(static_cast<float>(m_point0.y() - m_point2.y()));
  const __m128 h =
    _mm_sub_ps(_mm_set1_ps(static_cast<float>(m_point0.y())), _mm_load_ps(rays.originY.data()));
  const __m128 i = _mm_set1_ps(static_cast<float>(m_point0.z() - m_point1.z()));
  const __m128 j = _mm_set1_ps(static_cast<float>(m_point0.z() - m_point2.z()));
  const __m128 l =
    _mm_sub_ps(_mm_set1_ps(static_cast<float>(m_point0.z())), _mm_load_ps(rays.originZ.data()));
  const __m128 c = _mm_load_ps(rays.directionX.data());
  const __m128 g = _mm_load_ps(rays.directionY.data());
  const __m128 k = _mm_load_ps(rays.directionZ.data());

  const __m128 mm = _mm_sub_ps(_mm_mul_ps(f, k), _mm_mul_ps(g, j));
  const __m128 n = _mm_sub_ps(_mm_mul_ps(h, k), _mm_mul_ps(g, l));
  const __m128 p = _mm_sub_ps(_mm_mul_ps(f, l), _mm_mul_ps(h, j));
  const __m128 q = _mm_sub_ps(_mm_mul_ps(g, i), _mm_mul_ps(e, k));
  const __m128 r = _mm_sub_ps(_mm_mul_ps(e, l), _mm_mul_ps(h, i));
  const __m128 s = _mm_sub_ps(_mm_mul_ps(e, j), _mm_mul_ps(f, i));
  const __m128 denom =
    _mm_add_ps(_mm_add_ps(_mm_mul_ps(a, mm), _mm_mul_ps(b, q)), _mm_mul_ps(c, s));
  const __m128 invDenom = _mm_div_ps(_mm_set1_ps(1.0f), denom);

  const __m128 e1 = _mm_sub_ps(_mm_sub_ps(_mm_mul_ps(d, mm), _mm_mul_ps(b, n)), _mm_mul_ps(c, p));
  const __m128 beta = _mm_mul_ps(e1, invDenom);
  const __m128 e2 = _mm_add_ps(_mm_add_ps(_mm_mul_ps(a, n), _mm_mul_ps(d, q)), _mm_mul_ps(c, r));
  const __m128 gamma = _mm_mul_ps(e2, invDenom);
  const __m128 e3 = _mm_add_ps(_mm_sub_ps(_mm_mul_ps(a, p), _mm_mul_ps(b, r)), _mm_mul_ps(d, s));
  const __m128 t = _mm_mul_ps(e3, invDenom);

  const __m128 zero = _mm_setzero_ps();
  const __m128 one = _mm_set1_ps(1.0f);
  __m128 hit = _mm_and_ps(_mm_cmpge_ps(beta, zero), _mm_cmple_ps(beta, one));
  hit = _mm_and_ps(hit, _mm_cmpge_ps(gamma, zero));
  hit = _mm_and_ps(hit, _mm_cmple_ps(gamma, one));
  hit = _mm_and_ps(hit, _mm_cmple_ps(_mm_add_ps(beta, gamma), one));
  hit = _mm_and_ps(hit, _mm_cmpge_ps(t, zero));

  alignas(16) float distances[4];
  _mm_store_ps(distances, t);
  const int hitMask = _mm_movemask_ps(hit);
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
