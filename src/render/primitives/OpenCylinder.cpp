#include "render/State.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/primitives/OpenCylinder.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/Range.h"
#include "core/math/Quadric.h"
#include "core/math/HitPointInterval.h"
#include "core/geometry/Mesh.h"
#include <cmath>
#include <limits>

using namespace std;
using namespace render;

int OpenCylinder::solveSideHits(const Rayd& ray, double t[2]) const {
  double ox = ray.origin().x();
  double oz = ray.origin().z();
  double dx = ray.direction().x();
  double dz = ray.direction().z();

  double a = dx * dx + dz * dz;
  double b = 2.0 * (ox * dx + oz * dz);
  double c = ox * ox + oz * oz - m_radius * m_radius;

  return Quadric<double>(a, b, c).solveInto(t);
}

const Primitive* OpenCylinder::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                         render::State& state) const {
  double t[2] = {};
  int results = solveSideHits(ray, t);

  if (results < 2) {
    state.miss(this, "OpenCylinder, ray miss");
    return nullptr;
  } else {
    Range<double> yRange(-m_halfHeight, m_halfHeight);
    Vector3d point1 = ray.at(t[0]), point2 = ray.at(t[1]);

    if (yRange.contains(point1.y())) {
      Vector3d normal(point1.x() * m_invRadius, 0.0, point1.z() * m_invRadius);
      hitPoints.addIn(HitPoint(this, t[0], point1, normal, sideUvAt(point1)));
    }

    if (yRange.contains(point2.y())) {
      Vector3d normal(point2.x() * m_invRadius, 0.0, point2.z() * m_invRadius);
      hitPoints.addOut(HitPoint(this, t[1], point2, normal, sideUvAt(point2)));
    }

    if (t[0] <= 0 && t[1] <= 0) {
      state.miss(this, "OpenCylinder, behind ray");
      return nullptr;
    }

    if (hitPoints.empty()) {
      state.miss(this, "OpenCylinder, outside of y boundary");
      return nullptr;
    } else {
      state.hit(this, "OpenCylinder");
      return this;
    }
  }
}

template<typename Packet, typename StateArray, typename Result>
Result OpenCylinder::intersectPacketHitsFor(const Packet& rays, const StateArray& states) const {
  Result result;
  const Range<double> yRange(-m_halfHeight, m_halfHeight);
  for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
    if (!states[lane]) {
      continue;
    }
    State& state = *states[lane];
    const Rayd ray = rays.rayd(lane);

    double t[2] = {};
    const int roots = solveSideHits(ray, t);
    if (roots < 2) {
      state.miss(this, "OpenCylinder, ray miss");
      continue;
    }

    bool hasInRangeSideHit = false;
    double bestDistance = std::numeric_limits<double>::infinity();
    HitPoint bestHit;
    for (double distance : t) {
      const Vector3d point = ray.at(distance);
      if (!yRange.contains(point.y())) {
        continue;
      }

      hasInRangeSideHit = true;
      if (distance > 0.0 && distance < bestDistance) {
        bestDistance = distance;
        const Vector3d normal(point.x() * m_invRadius, 0.0, point.z() * m_invRadius);
        bestHit = HitPoint(this, distance, point, normal, sideUvAt(point));
      }
    }

    if (t[0] <= 0.0 && t[1] <= 0.0) {
      state.miss(this, "OpenCylinder, behind ray");
      continue;
    }

    if (!hasInRangeSideHit) {
      state.miss(this, "OpenCylinder, outside of y boundary");
      continue;
    }

    state.hit(this, "OpenCylinder");
    if (bestDistance < std::numeric_limits<double>::infinity()) {
      result.setHit(lane, this, bestHit);
    }
  }
  return result;
}

PrimitivePacketHit4 OpenCylinder::intersectPacketHits(const Ray4& rays,
                                                      const PrimitivePacketState4& states) const {
  return intersectPacketHitsFor<Ray4, PrimitivePacketState4, PrimitivePacketHit4>(rays, states);
}

PrimitivePacketHit8 OpenCylinder::intersectPacketHits(const Ray8& rays,
                                                      const PrimitivePacketState8& states) const {
  return intersectPacketHitsFor<Ray8, PrimitivePacketState8, PrimitivePacketHit8>(rays, states);
}

template<typename Packet, typename StateArray, typename Result>
Result OpenCylinder::intersectPacketIntervalsFor(const Packet& rays,
                                                 const StateArray& states) const {
  Result result;
  const Range<double> yRange(-m_halfHeight, m_halfHeight);
  for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
    if (!states[lane]) {
      continue;
    }
    State& state = *states[lane];
    const Rayd ray = rays.rayd(lane);

    double t[2] = {};
    const int roots = solveSideHits(ray, t);
    if (roots < 2) {
      state.miss(this, "OpenCylinder, ray miss");
      continue;
    }

    HitPointInterval hitPoints;
    const Vector3d point1 = ray.at(t[0]);
    const Vector3d point2 = ray.at(t[1]);

    if (yRange.contains(point1.y())) {
      const Vector3d normal(point1.x() * m_invRadius, 0.0, point1.z() * m_invRadius);
      hitPoints.addIn(HitPoint(this, t[0], point1, normal, sideUvAt(point1)));
    }

    if (yRange.contains(point2.y())) {
      const Vector3d normal(point2.x() * m_invRadius, 0.0, point2.z() * m_invRadius);
      hitPoints.addOut(HitPoint(this, t[1], point2, normal, sideUvAt(point2)));
    }

    if (t[0] <= 0.0 && t[1] <= 0.0) {
      if (!hitPoints.empty()) {
        result.setInterval(lane, nullptr, hitPoints);
      }
      state.miss(this, "OpenCylinder, behind ray");
      continue;
    }

    if (hitPoints.empty()) {
      state.miss(this, "OpenCylinder, outside of y boundary");
      continue;
    }

    result.setInterval(lane, this, hitPoints);
    state.hit(this, "OpenCylinder");
  }
  return result;
}

PrimitivePacketInterval4
OpenCylinder::intersectPacketIntervals(const Ray4& rays,
                                       const PrimitivePacketState4& states) const {
  return intersectPacketIntervalsFor<Ray4, PrimitivePacketState4, PrimitivePacketInterval4>(rays,
                                                                                            states);
}

PrimitivePacketInterval8
OpenCylinder::intersectPacketIntervals(const Ray8& rays,
                                       const PrimitivePacketState8& states) const {
  return intersectPacketIntervalsFor<Ray8, PrimitivePacketState8, PrimitivePacketInterval8>(rays,
                                                                                            states);
}

bool OpenCylinder::intersects(const Rayd& ray, render::State& state) const {
  double t[2] = {};
  int results = solveSideHits(ray, t);

  if (results < 2) {
    state.shadowMiss(this, "OpenCylinder, ray miss");
    return false;
  } else {
    if (t[0] <= 0 && t[1] <= 0) {
      state.shadowMiss(this, "OpenCylinder, behind ray");
      return false;
    }

    Range<double> yRange(-m_halfHeight, m_halfHeight);
    if ((t[0] > 0 && yRange.contains(ray.at(t[0]).y())) ||
        (t[1] > 0 && yRange.contains(ray.at(t[1]).y()))) {
      state.shadowHit(this, "OpenCylinder");
      return true;
    } else {
      state.shadowMiss(this, "OpenCylinder, outside of y boundary");
      return false;
    }
  }
}

void OpenCylinder::appendIntersectionSceneRecord(IntersectionSceneBuilder& builder,
                                                 const TransformedLeaf& leaf) const {
  builder.addOpenCylinder(leaf, m_radius, m_halfHeight);
}

BoundingBoxd OpenCylinder::calculateBoundingBox() const {
  return BoundingBoxd(Vector3d(-m_radius, -m_halfHeight, -m_radius),
                      Vector3d(m_radius, m_halfHeight, m_radius));
}

Vector3d OpenCylinder::farthestPoint(const Vector3d& direction) const {
  Vector3d planar = Vector3d(direction.x(), 0, direction.z());
  if (planar != Vector3d::null) {
    planar.normalize();
  }

  return Vector3d(planar.x() * m_radius, direction.y() < 0.0 ? -m_halfHeight : m_halfHeight,
                  planar.z() * m_radius);
}

Vector2d OpenCylinder::sideUvAt(const Vector3d& point) const {
  double u = std::atan2(point.z(), point.x()) / (2.0 * M_PI);
  if (u < 0.0) {
    u += 1.0;
  }

  const double height = 2.0 * m_halfHeight;
  const double v = height == 0.0 ? 0.0 : (point.y() + m_halfHeight) / height;
  return Vector2d(u, v);
}

std::shared_ptr<Mesh> OpenCylinder::tessellate(int lod) const {
  auto mesh = std::make_shared<Mesh>();

  int segments = 16 << lod;

  // Generate segments+1 pairs of vertices so u goes cleanly 0→1 with seam
  // duplication (first and last columns overlap in position but differ in UV).
  for (int i = 0; i <= segments; ++i) {
    double theta = 2.0 * M_PI * i / segments;
    double c = std::cos(theta), s = std::sin(theta);
    double u = static_cast<double>(i) / segments;

    Vector3d normal(c, 0.0, s);
    // bottom ring (v = 0), then top ring (v = 1) — interleaved: 2*i = bottom, 2*i+1 = top
    mesh->addVertex(Vector3d(m_radius * c, -m_halfHeight, m_radius * s), normal, Vector2d(u, 0.0));
    mesh->addVertex(Vector3d(m_radius * c, m_halfHeight, m_radius * s), normal, Vector2d(u, 1.0));
  }

  // Quads: each column i connects bottom[i]/top[i] to
  // bottom[i+1]/top[i+1]. Corners are listed CCW when viewed from
  // outside so face winding matches the radial vertex normals.
  for (int i = 0; i < segments; ++i) {
    int bl = 2 * i;           // bottom left
    int tl = 2 * i + 1;       // top left
    int br = 2 * (i + 1);     // bottom right
    int tr = 2 * (i + 1) + 1; // top right
    mesh->addFace({bl, tl, tr, br});
  }

  return mesh;
}
