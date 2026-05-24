#include "render/primitives/MeshTriangle.h"
#include "core/geometry/Mesh.h"
#include "core/math/RayPacket.h"
#include "render/State.h"
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

RayPacketIntersection4 MeshTriangle::intersectPacket(const Ray4& rays, render::State& state) const {
#ifndef __SSE__
  return Primitive::intersectPacket(rays, state);
#else
  RayPacketIntersection4 result;

  const Vector3d& v0 = m_mesh->vertices()[m_index0].point;
  const Vector3d& v1 = m_mesh->vertices()[m_index1].point;
  const Vector3d& v2 = m_mesh->vertices()[m_index2].point;

  const __m128 a = _mm_set1_ps(static_cast<float>(v0.x() - v1.x()));
  const __m128 b = _mm_set1_ps(static_cast<float>(v0.x() - v2.x()));
  const __m128 d =
    _mm_sub_ps(_mm_set1_ps(static_cast<float>(v0.x())), _mm_load_ps(rays.originX.data()));
  const __m128 e = _mm_set1_ps(static_cast<float>(v0.y() - v1.y()));
  const __m128 f = _mm_set1_ps(static_cast<float>(v0.y() - v2.y()));
  const __m128 h =
    _mm_sub_ps(_mm_set1_ps(static_cast<float>(v0.y())), _mm_load_ps(rays.originY.data()));
  const __m128 i = _mm_set1_ps(static_cast<float>(v0.z() - v1.z()));
  const __m128 j = _mm_set1_ps(static_cast<float>(v0.z() - v2.z()));
  const __m128 l =
    _mm_sub_ps(_mm_set1_ps(static_cast<float>(v0.z())), _mm_load_ps(rays.originZ.data()));
  const __m128 c = _mm_load_ps(rays.directionX.data());
  const __m128 g = _mm_load_ps(rays.directionY.data());
  const __m128 k = _mm_load_ps(rays.directionZ.data());

  const __m128 mm = _mm_sub_ps(_mm_mul_ps(f, k), _mm_mul_ps(g, j));
  const __m128 n = _mm_sub_ps(_mm_mul_ps(h, k), _mm_mul_ps(g, l));
  const __m128 p = _mm_sub_ps(_mm_mul_ps(f, l), _mm_mul_ps(h, j));
  const __m128 q = _mm_sub_ps(_mm_mul_ps(g, i), _mm_mul_ps(e, k));
  const __m128 r = _mm_sub_ps(_mm_mul_ps(e, l), _mm_mul_ps(h, i));
  const __m128 s = _mm_sub_ps(_mm_mul_ps(e, j), _mm_mul_ps(f, i));
  const __m128 invDenom =
    _mm_div_ps(_mm_set1_ps(1.0f),
               _mm_add_ps(_mm_add_ps(_mm_mul_ps(a, mm), _mm_mul_ps(b, q)), _mm_mul_ps(c, s)));

  const __m128 beta = _mm_mul_ps(
    _mm_sub_ps(_mm_sub_ps(_mm_mul_ps(d, mm), _mm_mul_ps(b, n)), _mm_mul_ps(c, p)), invDenom);
  const __m128 gamma = _mm_mul_ps(
    _mm_add_ps(_mm_add_ps(_mm_mul_ps(a, n), _mm_mul_ps(d, q)), _mm_mul_ps(c, r)), invDenom);
  const __m128 t = _mm_mul_ps(
    _mm_add_ps(_mm_sub_ps(_mm_mul_ps(a, p), _mm_mul_ps(b, r)), _mm_mul_ps(d, s)), invDenom);

  const __m128 zero = _mm_setzero_ps();
  const __m128 one = _mm_set1_ps(1.0f);
  __m128 hit = _mm_and_ps(_mm_cmpge_ps(beta, zero), _mm_cmple_ps(beta, one));
  hit = _mm_and_ps(hit, _mm_cmpge_ps(gamma, zero));
  hit = _mm_and_ps(hit, _mm_cmple_ps(_mm_add_ps(beta, gamma), one));
  hit = _mm_and_ps(hit, _mm_cmpge_ps(t, zero));

  alignas(16) float distances[4];
  _mm_store_ps(distances, t);
  const int hitMask = _mm_movemask_ps(hit);
  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    if ((hitMask & (1 << lane)) != 0) {
      result.setHit(lane, distances[lane], distances[lane]);
      packetHit(state, this, "MeshTriangle");
    } else {
      packetMiss(state, this, "MeshTriangle, ray miss");
    }
  }

  return result;
#endif
}

BoundingBoxd MeshTriangle::calculateBoundingBox() const {
  BoundingBoxd b;
  b.include(m_mesh->vertices()[m_index0].point);
  b.include(m_mesh->vertices()[m_index1].point);
  b.include(m_mesh->vertices()[m_index2].point);
  return b.grownByEpsilon();
}
