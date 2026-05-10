#include "render/State.h"
#include "render/primitives/OpenCylinder.h"
#include "core/math/Ray.h"
#include "core/math/Range.h"
#include "core/math/Quadric.h"
#include "core/math/HitPointInterval.h"
#include "core/geometry/Mesh.h"
#include <cmath>

using namespace std;
using namespace render;

const Primitive* OpenCylinder::intersect(const Rayd& ray, HitPointInterval& hitPoints, render::State& state) const {
  double ox = ray.origin().x();
  double oz = ray.origin().z();
  double dx = ray.direction().x();
  double dz = ray.direction().z();

  double a = dx * dx + dz * dz;
  double b = 2.0 * (ox * dx + oz * dz);
  double c = ox * ox + oz * oz - m_radius * m_radius;

  double t[2];
  int results = Quadric<double>(a, b, c).solveInto(t);

  if (results < 2) {
    state.miss(this, "OpenCylinder, ray miss");
    return nullptr;
  } else {
    Range<double> yRange(-m_halfHeight, m_halfHeight);
    Vector3d point1 = ray.at(t[0]),
             point2 = ray.at(t[1]);

    if (yRange.contains(point1.y())) {
      Vector3d normal(point1.x() * m_invRadius, 0.0, point1.z() * m_invRadius);
      hitPoints.addIn(HitPoint(this, t[0], point1, normal));
    }

    if (yRange.contains(point2.y())) {
      Vector3d normal(point2.x() * m_invRadius, 0.0, point2.z() * m_invRadius);
      hitPoints.addOut(HitPoint(this, t[1], point2, normal));
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

bool OpenCylinder::intersects(const Rayd& ray, render::State& state) const {
  double ox = ray.origin().x();
  double oz = ray.origin().z();
  double dx = ray.direction().x();
  double dz = ray.direction().z();

  double a = dx * dx + dz * dz;
  double b = 2.0 * (ox * dx + oz * dz);
  double c = ox * ox + oz * oz - m_radius * m_radius;

  double t[2];
  int results = Quadric<double>(a, b, c).solveInto(t);

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

BoundingBoxd OpenCylinder::calculateBoundingBox() const {
  return BoundingBoxd(
    Vector3d(-m_radius, -m_halfHeight, -m_radius),
    Vector3d( m_radius,  m_halfHeight,  m_radius)
  );
}

Vector3d OpenCylinder::farthestPoint(const Vector3d& direction) const {
  Vector3d planar = Vector3d(direction.x(), 0, direction.z());
  if (planar != Vector3d::null()) {
    planar.normalize();
  }

  return Vector3d(
    planar.x() * m_radius,
    direction.y() < 0.0 ? -m_halfHeight : m_halfHeight,
    planar.z() * m_radius
  );
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
    mesh->addVertex(Vector3d(m_radius * c,  m_halfHeight, m_radius * s), normal, Vector2d(u, 1.0));
  }

  // Quads: each column i connects bottom[i]/top[i] to
  // bottom[i+1]/top[i+1]. Corners are listed CCW when viewed from
  // outside so face winding matches the radial vertex normals.
  for (int i = 0; i < segments; ++i) {
    int bl = 2 * i;          // bottom left
    int tl = 2 * i + 1;     // top left
    int br = 2 * (i + 1);   // bottom right
    int tr = 2 * (i + 1) + 1; // top right
    mesh->addFace({bl, tl, tr, br});
  }

  return mesh;
}
