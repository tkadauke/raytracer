#include "raytracer/State.h"
#include "raytracer/primitives/Disk.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Number.h"
#include "core/geometry/Mesh.h"
#include <cmath>

using namespace raytracer;

const Primitive* Disk::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const {
  double t = (m_center - ray.origin()) * m_normal / (ray.direction() * m_normal);

  Vector4d hitPoint = ray.at(t);

  if (hitPoint.squaredDistanceTo(m_center) < m_squaredRadius) {
    if (-ray.direction() * m_normal < 0.0) {
      hitPoints.addOut(HitPoint(this, t, hitPoint, m_normal));
    } else {
      hitPoints.addIn(HitPoint(this, t, hitPoint, m_normal));
    }

    if (t < 0.0001) {
      state.miss(this, "Disk behind ray");
      return nullptr;
    } else {
      state.hit(this, "Disk");
      return this;
    }
  }

  state.miss(this, "Disk, ray miss");
  return nullptr;
}

BoundingBoxd Disk::calculateBoundingBox() const {
  Vector3d radius(m_radius, m_radius, m_radius);
  return BoundingBoxd(m_center - radius, m_center + radius);
}

Vector3d Disk::farthestPoint(const Vector3d& direction) const {
  Vector3d directionOnPlane = direction - m_normal * (direction * m_normal);
  if (isAlmostZero(directionOnPlane.length())) {
    return m_center;
  } else {
    return m_center + directionOnPlane.normalized() * m_radius;
  }
}

std::shared_ptr<Mesh> Disk::tessellate(int lod) const {
  auto mesh = std::make_shared<Mesh>();

  int segments = 16 << lod;

  // Build an orthonormal tangent frame in the disk's plane.
  Vector3d t1;
  if (std::fabs(m_normal.x()) < 0.9)
    t1 = (Vector3d(1, 0, 0) ^ m_normal).normalized();
  else
    t1 = (Vector3d(0, 1, 0) ^ m_normal).normalized();
  Vector3d t2 = (m_normal ^ t1).normalized();

  Vector3d center3(m_center);

  // Centre vertex (index 0)
  mesh->addVertex(center3, m_normal, Vector2d(0.5, 0.5));

  // Rim vertices (indices 1..segments)
  for (int i = 0; i < segments; ++i) {
    double theta = 2.0 * M_PI * i / segments;
    double c = std::cos(theta), s = std::sin(theta);
    Vector3d point = center3 + m_radius * (c * t1 + s * t2);
    mesh->addVertex(point, m_normal, Vector2d(0.5 + 0.5 * c, 0.5 + 0.5 * s));
  }

  // Triangle fan: centre, rim[i], rim[(i+1) % segments]
  for (int i = 0; i < segments; ++i) {
    int a = 1 + i;
    int b = 1 + (i + 1) % segments;
    mesh->addFace({0, a, b});
  }

  return mesh;
}
