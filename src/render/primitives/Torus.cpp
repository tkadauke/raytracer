#include "render/State.h"
#include "render/primitives/Torus.h"
#include "core/geometry/Mesh.h"
#include "core/math/Constants.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Quartic.h"
#include <cmath>
#include <limits>

using namespace std;
using namespace render;

const Primitive* Torus::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                  render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    state.miss(this, "Torus, bounding box miss");
    return nullptr;
  }

  const auto distances = sortedIntersectionDistances(ray);
  addIntersectionHits(ray, distances, hitPoints);

  if (hitPoints.empty()) {
    state.miss(this, "Torus, ray miss");
    return nullptr;
  }

  auto hitPoint = hitPoints.minWithPositiveDistance();
  if (hitPoint.isUndefined()) {
    state.miss(this, "Torus, behind Ray");
    return nullptr;
  } else {
    state.hit(this, "Torus");
    return this;
  }
}

PrimitivePacketHit4 Torus::intersectPacketHits(const Ray4& rays,
                                               const PrimitivePacketState4& states) const {
  PrimitivePacketHit4 result;
  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    State fallbackState;
    State& state = states[lane] ? *states[lane] : fallbackState;
    const Rayd ray = rays.rayd(lane);

    if (!boundingBoxIntersects(ray)) {
      state.miss(this, "Torus, bounding box miss");
      continue;
    }

    const auto distances = sortedIntersectionDistances(ray);
    const HitPoint hitPoint = closestPositiveHit(ray, distances);
    if (hitPoint.isUndefined()) {
      if (distances.size() == 2 || distances.size() == 4) {
        state.miss(this, "Torus, behind Ray");
      } else {
        state.miss(this, "Torus, ray miss");
      }
      continue;
    }

    result.setHit(lane, this, hitPoint);
    state.hit(this, "Torus");
  }
  return result;
}

shared_ptr<Mesh> Torus::tessellate(int lod) const {
  const int majorSegs = 16 << lod; // 16, 32, 64, …
  const int minorSegs = 16 << lod;

  auto mesh = make_shared<Mesh>();

  // Build a (majorSegs+1) × (minorSegs+1) vertex grid. Both seams (u=0/2π
  // and v=0/2π) are closed by duplicating the column/row at index 0 with
  // u=1 or v=1, giving continuous UV mapping across both wraps.
  for (int i = 0; i <= majorSegs; ++i) {
    double u = static_cast<double>(i) / majorSegs; // [0, 1]
    double angle_u = u * TAU;
    double cos_u = std::cos(angle_u);
    double sin_u = std::sin(angle_u);

    for (int j = 0; j <= minorSegs; ++j) {
      double v = static_cast<double>(j) / minorSegs; // [0, 1]
      double angle_v = v * TAU;
      double cos_v = std::cos(angle_v);
      double sin_v = std::sin(angle_v);

      // Parametric torus: (R + r·cosv)·(cosu, 0, sinu) + (0, r·sinv, 0)
      double dist = m_sweptRadius + m_tubeRadius * cos_v;
      Vector3d point(dist * cos_u, m_tubeRadius * sin_v, dist * sin_u);

      // Normal = direction from nearest major-circle point to surface point,
      // unit-length by construction: (cosv·cosu, sinv, cosv·sinu)
      Vector3d normal(cos_v * cos_u, sin_v, cos_v * sin_u);

      mesh->addVertex(point, normal, Vector2d(u, v));
    }
  }

  // One quad per (major-segment, minor-segment) cell, wound CCW from
  // outside so raster culling agrees with the parametric normals.
  for (int i = 0; i < majorSegs; ++i) {
    for (int j = 0; j < minorSegs; ++j) {
      int row = i * (minorSegs + 1);
      int nextRow = (i + 1) * (minorSegs + 1);
      mesh->addFace({row + j, row + j + 1, nextRow + j + 1, nextRow + j});
    }
  }

  return mesh;
}

BoundingBoxd Torus::calculateBoundingBox() const {
  Vector3d corner(m_sweptRadius + m_tubeRadius, m_tubeRadius, m_sweptRadius + m_tubeRadius);
  return BoundingBoxd(-corner, corner);
}

SortedResult<double, 4> Torus::sortedIntersectionDistances(const Rayd& ray) const {
  const Vector3d origin = ray.origin();
  const Vector3d direction = ray.direction();

  const double dd = direction * direction;
  const double oorr = origin * origin - m_sweptRadius * m_sweptRadius - m_tubeRadius * m_tubeRadius;
  const double od = origin * direction;
  const double fourRR = 4.0 * m_sweptRadius * m_sweptRadius;

  const double a = dd * dd;
  const double b = 4.0 * dd * od;
  const double c = 2.0 * dd * oorr + 4.0 * od * od + fourRR * direction.y() * direction.y();
  const double d = 4.0 * od * oorr + 2.0 * fourRR * origin.y() * direction.y();
  const double e = oorr * oorr - fourRR * (m_tubeRadius * m_tubeRadius - origin.y() * origin.y());

  Quartic<double> quartic(a, b, c, d, e);
  return quartic.shouldUseStableSolver() ? quartic.stableSortedResult() : quartic.sortedResult();
}

void Torus::addIntersectionHits(const Rayd& ray, const SortedResult<double, 4>& distances,
                                HitPointInterval& hitPoints) const {
  if (distances.size() == 2 || distances.size() == 4) {
    const Vector3d hitPoint1 = ray.at(distances[0]);
    const Vector3d hitPoint2 = ray.at(distances[1]);
    hitPoints.add(HitPoint(this, distances[0], hitPoint1, computeNormal(hitPoint1)),
                  HitPoint(this, distances[1], hitPoint2, computeNormal(hitPoint2)));
  }

  if (distances.size() == 4) {
    const Vector3d hitPoint1 = ray.at(distances[2]);
    const Vector3d hitPoint2 = ray.at(distances[3]);
    hitPoints.add(HitPoint(this, distances[2], hitPoint1, computeNormal(hitPoint1)),
                  HitPoint(this, distances[3], hitPoint2, computeNormal(hitPoint2)));
  }
}

HitPoint Torus::closestPositiveHit(const Rayd& ray,
                                   const SortedResult<double, 4>& distances) const {
  if (distances.size() != 2 && distances.size() != 4) {
    return HitPoint::undefined();
  }

  double closestDistance = std::numeric_limits<double>::infinity();
  HitPoint closestHit = HitPoint::undefined();
  for (double distance : distances) {
    if (distance <= 0.0 || distance >= closestDistance) {
      continue;
    }

    const Vector3d hitPoint = ray.at(distance);
    closestDistance = distance;
    closestHit = HitPoint(this, distance, hitPoint, computeNormal(hitPoint));
  }

  return closestHit;
}

Vector3d Torus::computeNormal(const Vector3d& p) const {
  double paramSquared = m_sweptRadius * m_sweptRadius + m_tubeRadius * m_tubeRadius;
  double sumSquared = p * p;

  Vector3d result(4.0 * p.x() * (sumSquared - paramSquared),
                  4.0 * p.y() * (sumSquared - paramSquared + 2.0 * m_sweptRadius * m_sweptRadius),
                  4.0 * p.z() * (sumSquared - paramSquared));

  return result.normalized();
}
