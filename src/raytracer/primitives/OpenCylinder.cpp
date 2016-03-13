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
    if (t[0] <= 0 && t[1] <= 0)
      return nullptr;

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
    if (t[0] <= 0 && t[1] <= 0)
      return false;

    Range<double> yRange(-m_halfHeight, m_halfHeight);
    return yRange.contains(ray.at(t[0]).y()) ||
           yRange.contains(ray.at(t[1]).y());
  }
}

BoundingBoxd OpenCylinder::boundingBox() {
  return BoundingBoxd(
    Vector3d(-m_radius, -m_halfHeight, -m_radius),
    Vector3d( m_radius,  m_halfHeight,  m_radius)
  );
}
