#include "render/primitives/MeshTriangle.h"
#include "core/geometry/Mesh.h"
#include "core/math/RayPacket.h"
#include "core/simd/Float4.h"
#include "render/State.h"

using namespace render;

void MeshTriangle::recordPacketHit(State& state, const char* reason) const {
  if (state.traceEvents) {
    state.hit(this, reason);
  } else {
    ++state.intersectionHits;
  }
}

void MeshTriangle::recordPacketMiss(State& state, const char* reason) const {
  if (state.traceEvents) {
    state.miss(this, reason);
  } else {
    ++state.intersectionMisses;
  }
}

double MeshTriangle::minimumHitDistance() const {
  return 0.0;
}

Vector2d MeshTriangle::uvAtBarycentric(double beta, double gamma) const {
  const double alpha = 1.0 - beta - gamma;
  return m_mesh->vertices()[m_index0].uv * alpha + m_mesh->vertices()[m_index1].uv * beta +
         m_mesh->vertices()[m_index2].uv * gamma;
}

HitPoint MeshTriangle::materializeHitPoint(const Rayd& ray, double distance, double beta,
                                           double gamma) const {
  return HitPoint(this, distance, ray.at(distance), normalAtBarycentric(beta, gamma),
                  uvAtBarycentric(beta, gamma));
}

MeshTriangle::PacketBarycentricIntersection4
MeshTriangle::intersectPacketBarycentric(const Ray4& rays) const {
  using namespace core::simd;

  PacketBarycentricIntersection4 result;
  const Vector3d& v0 = m_mesh->vertices()[m_index0].point;
  const Vector3d& v1 = m_mesh->vertices()[m_index1].point;
  const Vector3d& v2 = m_mesh->vertices()[m_index2].point;

  const Float4 a = set1(static_cast<float>(v0.x() - v1.x()));
  const Float4 b = set1(static_cast<float>(v0.x() - v2.x()));
  const Float4 c = load4(rays.directionX.data());
  const Float4 d = set1(static_cast<float>(v0.x())) - load4(rays.originX.data());
  const Float4 e = set1(static_cast<float>(v0.y() - v1.y()));
  const Float4 f = set1(static_cast<float>(v0.y() - v2.y()));
  const Float4 g = load4(rays.directionY.data());
  const Float4 h = set1(static_cast<float>(v0.y())) - load4(rays.originY.data());
  const Float4 i = set1(static_cast<float>(v0.z() - v1.z()));
  const Float4 j = set1(static_cast<float>(v0.z() - v2.z()));
  const Float4 k = load4(rays.directionZ.data());
  const Float4 l = set1(static_cast<float>(v0.z())) - load4(rays.originZ.data());

  const Float4 m = f * k - g * j;
  const Float4 n = h * k - g * l;
  const Float4 p = f * l - h * j;
  const Float4 q = g * i - e * k;
  const Float4 r = e * l - h * i;
  const Float4 s = e * j - f * i;
  const Float4 invDenom = set1(1.0f) / (a * m + b * q + c * s);

  const Float4 beta = (d * m - b * n - c * p) * invDenom;
  const Float4 gamma = (a * n + d * q + c * r) * invDenom;
  const Float4 distance = (a * p - b * r + d * s) * invDenom;

  const Float4 zeroValue = zero();
  const Float4 oneValue = set1(1.0f);
  const Float4 minimumDistance = set1(static_cast<float>(minimumHitDistance()));
  Mask4 hit = maskAnd(cmpGe(beta, zeroValue), cmpLe(beta, oneValue));
  hit = maskAnd(hit, cmpGe(gamma, zeroValue));
  hit = maskAnd(hit, cmpLe(beta + gamma, oneValue));
  hit = maskAnd(hit, cmpGe(distance, minimumDistance));

  store4(result.distances.data(), distance);
  store4(result.betas.data(), beta);
  store4(result.gammas.data(), gamma);
  result.hitMask = movemask(hit);
  return result;
}

RayPacketIntersection4 MeshTriangle::intersectPacket(const Ray4& rays, render::State& state) const {
  RayPacketIntersection4 result;
  const PacketBarycentricIntersection4 intersections = intersectPacketBarycentric(rays);

  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    if ((intersections.hitMask & (1 << lane)) != 0) {
      result.setHit(lane, intersections.distances[lane], intersections.distances[lane]);
      recordPacketHit(state, "MeshTriangle");
    } else {
      recordPacketMiss(state, "MeshTriangle, ray miss");
    }
  }

  return result;
}

PrimitivePacketHit4 MeshTriangle::intersectPacketHits(const Ray4& rays,
                                                      const PrimitivePacketState4& states) const {
  PrimitivePacketHit4 result;
  const PacketBarycentricIntersection4 intersections = intersectPacketBarycentric(rays);

  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    State fallbackState;
    State& state = states[lane] ? *states[lane] : fallbackState;
    if ((intersections.hitMask & (1 << lane)) == 0) {
      recordPacketMiss(state, "MeshTriangle, ray miss");
      continue;
    }

    result.setHit(lane, this,
                  materializeHitPoint(rays.rayd(lane), intersections.distances[lane],
                                      intersections.betas[lane], intersections.gammas[lane]));
    recordPacketHit(state, "MeshTriangle");
  }
  return result;
}

BoundingBoxd MeshTriangle::calculateBoundingBox() const {
  BoundingBoxd b;
  b.include(m_mesh->vertices()[m_index0].point);
  b.include(m_mesh->vertices()[m_index1].point);
  b.include(m_mesh->vertices()[m_index2].point);
  return b.grownByEpsilon();
}
