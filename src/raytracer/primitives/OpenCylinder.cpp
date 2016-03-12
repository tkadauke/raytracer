#include "raytracer/primitives/OpenCylinder.h"
#include "core/math/Ray.h"
#include "core/math/Range.h"
#include "core/math/Quadric.h"
#include "core/math/HitPointInterval.h"
#include <cmath>

using namespace std;
using namespace raytracer;

Primitive* OpenCylinder::intersect(const Rayd& ray, HitPointInterval& hitPoints) {
  double ox = ray.origin().x();
  double oz = ray.origin().z();
  double dx = ray.direction().x();
  double dz = ray.direction().z();

  double a = dx * dx + dz * dz;
  double b = 2.0 * (ox * dx + oz * dz);
  double c = ox * ox + oz * oz - m_radius * m_radius;
  
  double t[2];
  int results = Quadric<double>(a, b, c).solveInto(t);
  
  if (results == 0) {
    return nullptr;
  } else {
    Range<double> yRange(-m_height, m_height);
    Vector3d point = ray.at(t[0]);

    if (yRange.contains(point.y())) {
      Vector3d normal(point.x() * m_invRadius, 0.0, point.z() * m_invRadius);

      if (-ray.direction() * normal < 0.0)
        normal = -normal;

      hitPoints.add(HitPoint(this, t[0], point, normal));
    }

    if (results == 2) {
      point = ray.at(t[1]);

      if (yRange.contains(point.y())) {
        Vector3d normal(point.x() * m_invRadius, 0.0, point.z() * m_invRadius);

        if (-ray.direction() * normal < 0.0)
          normal = -normal;

        hitPoints.add(HitPoint(this, t[1], point, normal));
      }
    }
    
    return hitPoints.empty() ? nullptr : this;
  }
}

bool OpenCylinder::intersects(const Rayd& ray) {
  double ox = ray.origin().x();
  double oz = ray.origin().z();
  double dx = ray.direction().x();
  double dz = ray.direction().z();

  double a = dx * dx + dz * dz;
  double b = 2.0 * (ox * dx + oz * dz);
  double c = ox * ox + oz * oz - m_radius * m_radius;
  
  double t[2];
  int results = Quadric<double>(a, b, c).solveInto(t);
  
  if (results == 0) {
    return false;
  } else {
    Range<double> yRange(-m_height, m_height);
    return yRange.contains(ray.at(t[0]).y()) ||
      (results == 2 && yRange.contains(ray.at(t[1]).y()));
  }
}

BoundingBoxd OpenCylinder::boundingBox() {
  return BoundingBoxd(
    Vector3d(-m_radius, -m_height, -m_radius),
    Vector3d( m_radius,  m_height,  m_radius)
  );
}
