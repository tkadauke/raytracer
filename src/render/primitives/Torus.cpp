#include "render/State.h"
#include "render/primitives/Torus.h"
#include "core/geometry/Mesh.h"
#include "core/math/Constants.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Quartic.h"
#include <cmath>

using namespace std;
using namespace render;

const Primitive* Torus::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                  render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    state.miss(this, "Torus, bounding box miss");
    return nullptr;
  }

  Vector3d origin = ray.origin();
  Vector3d direction = ray.direction();

  double dd = direction * direction;
  double oorr = origin * origin - m_sweptRadius * m_sweptRadius - m_tubeRadius * m_tubeRadius;
  double od = origin * direction;
  double fourRR = 4.0 * m_sweptRadius * m_sweptRadius;

  double a = dd * dd;
  double b = 4.0 * dd * od;
  double c = 2.0 * dd * oorr + 4.0 * od * od + fourRR * direction.y() * direction.y();
  double d = 4.0 * od * oorr + 2.0 * fourRR * origin.y() * direction.y();
  double e = oorr * oorr - fourRR * (m_tubeRadius * m_tubeRadius - origin.y() * origin.y());

  Quartic<double> quartic(a, b, c, d, e);

  auto results =
    quartic.shouldUseStableSolver() ? quartic.stableSortedResult() : quartic.sortedResult();

  if (results.size() == 2 || results.size() == 4) {
    Vector3d hitPoint1 = ray.at(results[0]), hitPoint2 = ray.at(results[1]);
    hitPoints.add(HitPoint(this, results[0], hitPoint1, computeNormal(hitPoint1)),
                  HitPoint(this, results[1], hitPoint2, computeNormal(hitPoint2)));
  }

  if (results.size() == 4) {
    Vector3d hitPoint1 = ray.at(results[2]), hitPoint2 = ray.at(results[3]);
    hitPoints.add(HitPoint(this, results[2], hitPoint1, computeNormal(hitPoint1)),
                  HitPoint(this, results[3], hitPoint2, computeNormal(hitPoint2)));
  }

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

Vector3d Torus::computeNormal(const Vector3d& p) const {
  double paramSquared = m_sweptRadius * m_sweptRadius + m_tubeRadius * m_tubeRadius;
  double sumSquared = p * p;

  Vector3d result(4.0 * p.x() * (sumSquared - paramSquared),
                  4.0 * p.y() * (sumSquared - paramSquared + 2.0 * m_sweptRadius * m_sweptRadius),
                  4.0 * p.z() * (sumSquared - paramSquared));

  return result.normalized();
}
