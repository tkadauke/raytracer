#include "render/State.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/primitives/Box.h"
#include "PacketStateHelpers.h"
#include "core/SimdFeatures.h"
#include "core/geometry/Mesh.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include "core/simd/Float4.h"
#include <cmath>
#include <limits>

using namespace std;
using namespace render;

namespace {
  double normalizedFaceCoordinate(double value, double halfExtent) {
    if (halfExtent == 0.0) {
      return 0.0;
    }
    return (value + halfExtent) / (2.0 * halfExtent);
  }

  double invertedFaceCoordinate(double value, double halfExtent) {
    if (halfExtent == 0.0) {
      return 0.0;
    }
    return (halfExtent - value) / (2.0 * halfExtent);
  }

  Vector2d boxFaceUv(const Vector3d& center, const Vector3d& edge, const Vector4d& point,
                     const Vector3d& normal) {
    const double x = point.x() - center.x();
    const double y = point.y() - center.y();
    const double z = point.z() - center.z();

    if (std::fabs(normal.x()) > 0.5) {
      if (normal.x() > 0.0) {
        return Vector2d(normalizedFaceCoordinate(y, edge.y()),
                        normalizedFaceCoordinate(z, edge.z()));
      }
      return Vector2d(normalizedFaceCoordinate(z, edge.z()), normalizedFaceCoordinate(y, edge.y()));
    }

    if (std::fabs(normal.y()) > 0.5) {
      if (normal.y() > 0.0) {
        return Vector2d(normalizedFaceCoordinate(z, edge.z()),
                        normalizedFaceCoordinate(x, edge.x()));
      }
      return Vector2d(normalizedFaceCoordinate(x, edge.x()), normalizedFaceCoordinate(z, edge.z()));
    }

    if (normal.z() > 0.0) {
      return Vector2d(normalizedFaceCoordinate(x, edge.x()), normalizedFaceCoordinate(y, edge.y()));
    }
    return Vector2d(normalizedFaceCoordinate(x, edge.x()), invertedFaceCoordinate(y, edge.y()));
  }
}

const Primitive* Box::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                render::State& state) const {
  int parallel = 0;
  bool found = false;
  Vector3d d = m_center - ray.origin();
  double t1 = 0, t2 = 0;
  Vector3d normal1, normal2;

  for (int i = 0; i < 3; ++i) {
    if (fabs(ray.direction()[i]) < 0.0001) {
      parallel |= 1 << i;
    } else {
      double dir = (ray.direction()[i] > 0.0) ? 1.0 : -1.0;
      double es = (ray.direction()[i] > 0.0) ? m_edge[i] : -m_edge[i];
      double invDi = 1.0 / ray.direction()[i];

      if (!found) {
        normal1[i] = -dir;
        normal2[i] = dir;
        t1 = (d[i] - es) * invDi;
        t2 = (d[i] + es) * invDi;
        found = true;
      } else {
        double s = (d[i] - es) * invDi;
        if (s > t1) {
          normal1 = Vector3d();
          normal1[i] = -dir;
          t1 = s;
        }
        s = (d[i] + es) * invDi;
        if (s < t2) {
          normal2 = Vector3d();
          normal2[i] = dir;
          t2 = s;
        }
        if (t1 > t2) {
          state.miss(this, "Box, ray miss");
          return nullptr;
        }
      }
    }
  }

  if (parallel)
    for (int i = 0; i < 3; ++i)
      if (parallel & (1 << i))
        if (fabs(d[i] - t1 * ray.direction()[i]) > m_edge[i] ||
            fabs(d[i] - t2 * ray.direction()[i]) > m_edge[i]) {
          state.miss(this, "Box, ray parallel");
          return nullptr;
        }

  const Vector4d point1 = ray.at(t1);
  const Vector4d point2 = ray.at(t2);
  hitPoints.add(HitPoint(this, t1, point1, normal1, boxFaceUv(m_center, m_edge, point1, normal1)),
                HitPoint(this, t2, point2, normal2, boxFaceUv(m_center, m_edge, point2, normal2)));

  if (t1 < 0 && t2 < 0) {
    state.miss(this, "Box, behind ray");
    return nullptr;
  }

  state.hit(this, "Box");
  return this;
}

RayPacketIntersection4 Box::intersectPacket(const Ray4& rays, render::State& state) const {
#if !(RAYTRACER_SIMD_SSE || RAYTRACER_SIMD_NEON)
  return Primitive::intersectPacket(rays, state);
#else
  using namespace core::simd;

  RayPacketIntersection4 result;

  const Float4 zeroValue = zero();
  const Float4 one = set1(1.0f);
  const Float4 negInfinity = set1(-std::numeric_limits<float>::infinity());
  const Float4 posInfinity = set1(std::numeric_limits<float>::infinity());
  const Vector3d min = m_center - m_edge;
  const Vector3d max = m_center + m_edge;

  auto axis = [&](const Ray4::LaneArray& origins, const Ray4::LaneArray& directions, float minValue,
                  float maxValue, Float4& enter, Float4& exit, Mask4& valid) {
    const Float4 o = load4(origins.data());
    const Float4 d = load4(directions.data());
    const Float4 minv = set1(minValue);
    const Float4 maxv = set1(maxValue);
    const Mask4 parallel = cmpEq(d, zeroValue);
    const Mask4 inside = maskAnd(cmpGe(o, minv), cmpLe(o, maxv));
    const Float4 invD = one / d;
    const Float4 t1 = (minv - o) * invD;
    const Float4 t2 = (maxv - o) * invD;
    enter = core::simd::max(enter, select(parallel, negInfinity, core::simd::min(t1, t2)));
    exit = core::simd::min(exit, select(parallel, posInfinity, core::simd::max(t1, t2)));
    valid = maskAnd(valid, maskOr(maskAndNot(parallel, cmpEq(d, d)), maskAnd(parallel, inside)));
  };

  Float4 enter = negInfinity;
  Float4 exit = posInfinity;
  Mask4 valid = cmpEq(zeroValue, zeroValue);
  axis(rays.originX, rays.directionX, static_cast<float>(min.x()), static_cast<float>(max.x()),
       enter, exit, valid);
  axis(rays.originY, rays.directionY, static_cast<float>(min.y()), static_cast<float>(max.y()),
       enter, exit, valid);
  axis(rays.originZ, rays.directionZ, static_cast<float>(min.z()), static_cast<float>(max.z()),
       enter, exit, valid);

  const Mask4 hit = maskAnd(valid, maskAnd(cmpLe(enter, exit), cmpGe(exit, zeroValue)));
  alignas(16) float nearDistances[4];
  alignas(16) float farDistances[4];
  store4(nearDistances, enter);
  store4(farDistances, exit);

  const int hitMask = movemask(hit);
  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    if ((hitMask & (1 << lane)) != 0) {
      result.setHit(lane, nearDistances[lane], farDistances[lane]);
      packetHit(state, this, "Box");
    } else {
      packetMiss(state, this, "Box, ray miss");
    }
  }

  return result;
#endif
}

template<typename Packet, typename StateArray, typename Result>
Result Box::intersectPacketHitsFor(const Packet& rays, const StateArray& states) const {
  Result result;
  for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
    if (!states[lane]) {
      continue;
    }
    State& state = *states[lane];
    const Rayd ray = rays.rayd(lane);

    int parallel = 0;
    bool found = false;
    bool rejected = false;
    const Vector3d d = m_center - ray.origin();
    double t1 = 0.0;
    double t2 = 0.0;
    Vector3d normal1;
    Vector3d normal2;

    for (int i = 0; i < 3; ++i) {
      if (fabs(ray.direction()[i]) < 0.0001) {
        parallel |= 1 << i;
      } else {
        const double dir = (ray.direction()[i] > 0.0) ? 1.0 : -1.0;
        const double es = (ray.direction()[i] > 0.0) ? m_edge[i] : -m_edge[i];
        const double invDi = 1.0 / ray.direction()[i];

        if (!found) {
          normal1[i] = -dir;
          normal2[i] = dir;
          t1 = (d[i] - es) * invDi;
          t2 = (d[i] + es) * invDi;
          found = true;
        } else {
          const double s1 = (d[i] - es) * invDi;
          if (s1 > t1) {
            normal1 = Vector3d();
            normal1[i] = -dir;
            t1 = s1;
          }
          const double s2 = (d[i] + es) * invDi;
          if (s2 < t2) {
            normal2 = Vector3d();
            normal2[i] = dir;
            t2 = s2;
          }
          if (t1 > t2) {
            state.miss(this, "Box, ray miss");
            rejected = true;
            break;
          }
        }
      }
    }

    if (rejected) {
      continue;
    }
    if (!found) {
      state.miss(this, "Box, ray parallel");
      continue;
    }

    bool parallelMiss = false;
    for (int i = 0; i < 3; ++i) {
      if ((parallel & (1 << i)) && (fabs(d[i] - t1 * ray.direction()[i]) > m_edge[i] ||
                                    fabs(d[i] - t2 * ray.direction()[i]) > m_edge[i])) {
        parallelMiss = true;
      }
    }
    if (parallelMiss) {
      state.miss(this, "Box, ray parallel");
      continue;
    }

    if (t1 < 0.0 && t2 < 0.0) {
      state.miss(this, "Box, behind ray");
      continue;
    }

    state.hit(this, "Box");
    const double t = t1 > 0.0 ? t1 : t2;
    const Vector3d normal = t1 > 0.0 ? normal1 : normal2;
    if (t > 0.0) {
      const Vector4d point = ray.at(t);
      result.setHit(lane, this,
                    HitPoint(this, t, point, normal, boxFaceUv(m_center, m_edge, point, normal)));
    }
  }
  return result;
}

PrimitivePacketHit4 Box::intersectPacketHits(const Ray4& rays,
                                             const PrimitivePacketState4& states) const {
  return intersectPacketHitsFor<Ray4, PrimitivePacketState4, PrimitivePacketHit4>(rays, states);
}

PrimitivePacketHit8 Box::intersectPacketHits(const Ray8& rays,
                                             const PrimitivePacketState8& states) const {
  return intersectPacketHitsFor<Ray8, PrimitivePacketState8, PrimitivePacketHit8>(rays, states);
}

template<typename Packet, typename StateArray, typename Result>
Result Box::intersectPacketIntervalsFor(const Packet& rays, const StateArray& states) const {
  Result result;
  for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
    if (!states[lane]) {
      continue;
    }
    State& state = *states[lane];
    const Rayd ray = rays.rayd(lane);

    int parallel = 0;
    bool found = false;
    bool rejected = false;
    const Vector3d d = m_center - ray.origin();
    double t1 = 0.0;
    double t2 = 0.0;
    Vector3d normal1;
    Vector3d normal2;

    for (int i = 0; i < 3; ++i) {
      if (fabs(ray.direction()[i]) < 0.0001) {
        parallel |= 1 << i;
      } else {
        const double dir = (ray.direction()[i] > 0.0) ? 1.0 : -1.0;
        const double es = (ray.direction()[i] > 0.0) ? m_edge[i] : -m_edge[i];
        const double invDi = 1.0 / ray.direction()[i];

        if (!found) {
          normal1[i] = -dir;
          normal2[i] = dir;
          t1 = (d[i] - es) * invDi;
          t2 = (d[i] + es) * invDi;
          found = true;
        } else {
          const double s1 = (d[i] - es) * invDi;
          if (s1 > t1) {
            normal1 = Vector3d();
            normal1[i] = -dir;
            t1 = s1;
          }
          const double s2 = (d[i] + es) * invDi;
          if (s2 < t2) {
            normal2 = Vector3d();
            normal2[i] = dir;
            t2 = s2;
          }
          if (t1 > t2) {
            state.miss(this, "Box, ray miss");
            rejected = true;
            break;
          }
        }
      }
    }

    if (rejected) {
      continue;
    }
    if (!found) {
      state.miss(this, "Box, ray parallel");
      continue;
    }

    bool parallelMiss = false;
    for (int i = 0; i < 3; ++i) {
      if ((parallel & (1 << i)) && (fabs(d[i] - t1 * ray.direction()[i]) > m_edge[i] ||
                                    fabs(d[i] - t2 * ray.direction()[i]) > m_edge[i])) {
        parallelMiss = true;
      }
    }
    if (parallelMiss) {
      state.miss(this, "Box, ray parallel");
      continue;
    }

    const Vector4d point1 = ray.at(t1);
    const Vector4d point2 = ray.at(t2);
    HitPointInterval hitPoints(
      HitPoint(this, t1, point1, normal1, boxFaceUv(m_center, m_edge, point1, normal1)),
      HitPoint(this, t2, point2, normal2, boxFaceUv(m_center, m_edge, point2, normal2)));

    if (t1 < 0.0 && t2 < 0.0) {
      result.setInterval(lane, nullptr, hitPoints);
      state.miss(this, "Box, behind ray");
      continue;
    }

    result.setInterval(lane, this, hitPoints);
    state.hit(this, "Box");
  }
  return result;
}

PrimitivePacketInterval4 Box::intersectPacketIntervals(const Ray4& rays,
                                                       const PrimitivePacketState4& states) const {
  return intersectPacketIntervalsFor<Ray4, PrimitivePacketState4, PrimitivePacketInterval4>(rays,
                                                                                            states);
}

PrimitivePacketInterval8 Box::intersectPacketIntervals(const Ray8& rays,
                                                       const PrimitivePacketState8& states) const {
  return intersectPacketIntervalsFor<Ray8, PrimitivePacketState8, PrimitivePacketInterval8>(rays,
                                                                                            states);
}

bool Box::intersects(const Rayd& ray, render::State&) const {
  return boundingBox().intersects(ray);
}

BoundingBoxd Box::calculateBoundingBox() const {
  return BoundingBoxd(m_center - m_edge, m_center + m_edge);
}

Vector3d Box::farthestPoint(const Vector3d& direction) const {
  return m_center + Vector3d(direction.x() < 0.0 ? -m_edge.x() : m_edge.x(),
                             direction.y() < 0.0 ? -m_edge.y() : m_edge.y(),
                             direction.z() < 0.0 ? -m_edge.z() : m_edge.z());
}

void Box::appendIntersectionSceneRecord(IntersectionSceneBuilder& builder,
                                        const TransformedLeaf& leaf) const {
  const std::shared_ptr<Mesh> mesh = tessellate(0);
  builder.addMeshTriangles(leaf, *mesh);
}

std::shared_ptr<Mesh> Box::tessellate(int) const {
  // Box is polyhedral — every level of detail produces the same six-face
  // mesh. 24 vertices total: each face contributes its own four because
  // sharing across faces would force a single normal/UV per shared corner,
  // which is wrong for both flat shading and per-face texturing.
  auto mesh = std::make_shared<Mesh>();

  const double cx = m_center.x(), cy = m_center.y(), cz = m_center.z();
  const double ex = m_edge.x(), ey = m_edge.y(), ez = m_edge.z();

  // Helper: add four vertices for one face plus the quad face index.
  // `addQuad` picks UVs per corner so each face's texture spans
  // exactly [0, 1]² with `u` along the first cross-edge and `v`
  // along the second. Face winding goes CCW when viewed from
  // outside the box — adopting the OpenGL / glTF convention so a
  // future rasterizer can back-face-cull on the standard sign.
  auto addQuad = [&](const Vector3d& normal, const Vector3d& v0, const Vector3d& v1,
                     const Vector3d& v2, const Vector3d& v3) {
    int base = static_cast<int>(mesh->vertices().size());
    mesh->addVertex(v0, normal, Vector2d(0, 0));
    mesh->addVertex(v1, normal, Vector2d(1, 0));
    mesh->addVertex(v2, normal, Vector2d(1, 1));
    mesh->addVertex(v3, normal, Vector2d(0, 1));
    mesh->addFace({base, base + 1, base + 2, base + 3});
  };

  // The eight corners. Naming `pXYZ` where each {0,1} is the sign of
  // (-edge, +edge) along that axis.
  const Vector3d p000(cx - ex, cy - ey, cz - ez);
  const Vector3d p001(cx - ex, cy - ey, cz + ez);
  const Vector3d p010(cx - ex, cy + ey, cz - ez);
  const Vector3d p011(cx - ex, cy + ey, cz + ez);
  const Vector3d p100(cx + ex, cy - ey, cz - ez);
  const Vector3d p101(cx + ex, cy - ey, cz + ez);
  const Vector3d p110(cx + ex, cy + ey, cz - ez);
  const Vector3d p111(cx + ex, cy + ey, cz + ez);

  // Faces, each with its outward normal. The four corners are listed
  // CCW from outside; pick `u`-direction first then `v`-direction.
  // Vector3d::up() in this codebase is (0, -1, 0), so the "+y face"
  // (mathematically the bottom of the box in world coords) and the
  // "-y face" (top) follow that convention; see the existing
  // intersect() above for the matching sign discipline.

  // +X face — outward normal (+1, 0, 0). Viewed from +X looking
  // toward -X: +Y is to the right (in math), +Z is up.
  addQuad(Vector3d(1, 0, 0), p100, p110, p111, p101);
  // -X face — outward (-1, 0, 0). Viewed from -X looking toward +X:
  // +Z is to the right, +Y is up.
  addQuad(Vector3d(-1, 0, 0), p000, p001, p011, p010);

  // +Y face — outward (0, +1, 0). World convention: +Y is "down."
  // Viewed from +Y: +X to the right, +Z up.
  addQuad(Vector3d(0, 1, 0), p010, p011, p111, p110);
  // -Y face — outward (0, -1, 0). Viewed from -Y: +Z to the right,
  // +X up.
  addQuad(Vector3d(0, -1, 0), p000, p100, p101, p001);

  // +Z face — outward (0, 0, +1). Viewed from +Z: +X to the right,
  // +Y up.
  addQuad(Vector3d(0, 0, 1), p001, p101, p111, p011);
  // -Z face — outward (0, 0, -1). Viewed from -Z: +X to the left,
  // +Y up.
  addQuad(Vector3d(0, 0, -1), p010, p110, p100, p000);

  return mesh;
}
