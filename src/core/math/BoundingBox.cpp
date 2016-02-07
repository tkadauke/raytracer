#include "core/math/BoundingBox.h"
#include "core/math/Ray.h"

const BoundingBox& BoundingBox::undefined() {
  static BoundingBox b(Vector3d::undefined(), Vector3d::undefined());
  return b;
}

bool BoundingBox::intersects(const Ray& ray) const {
  double ox = ray.origin().x();
  double oy = ray.origin().y();
  double oz = ray.origin().z();
  double dx = ray.direction().x();
  double dy = ray.direction().y();
  double dz = ray.direction().z();
  
  double xMin, yMin, zMin;
  double xMax, yMax, zMax;
  
  double a = 1.0 / dx;
  if (a >= 0) {
    xMin = (m_min.x() - ox) * a;
    xMax = (m_max.x() - ox) * a;
  } else {
    xMin = (m_max.x() - ox) * a;
    xMax = (m_min.x() - ox) * a;
  }
  
  double b = 1.0 / dy;
  if (b >= 0) {
    yMin = (m_min.y() - oy) * b;
    yMax = (m_max.y() - oy) * b;
  } else {
    yMin = (m_max.y() - oy) * b;
    yMax = (m_min.y() - oy) * b;
  }
  
  double c = 1.0 / dz;
  if (c >= 0) {
    zMin = (m_min.z() - oz) * c;
    zMax = (m_max.z() - oz) * c;
  } else {
    zMin = (m_max.z() - oz) * c;
    zMax = (m_min.z() - oz) * c;
  }
  
  double t0, t1;
  
  if (xMin > yMin)
    t0 = xMin;
  else
    t0 = yMin;
  
  if (zMin > t0)
    t0 = zMin;
  
  if (xMax < yMax)
    t1 = xMax;
  else
    t1 = yMax;
  
  if (zMax < t1)
    t1 = zMax;
  
  return (t0 < t1 && t1 > Ray::epsilon);
}
