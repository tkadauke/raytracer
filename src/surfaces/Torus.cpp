#include "surfaces/Torus.h"
#include "math/Ray.h"
#include "math/HitPointInterval.h"
#include "math/Quartic.h"
#include <cmath>
#include <iostream>

using namespace std;

Surface* Torus::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  if (!boundingBox().intersects(ray))
    return 0;
  
  Vector3d origin = ray.origin();
  Vector3d direction = ray.direction();
  
  double dd = direction * direction;
  double oorr = origin * origin - m_sweptRadius * m_sweptRadius - m_tubeRadius * m_tubeRadius;
  double od = origin * direction;
  double fourRR = 4.0 * m_sweptRadius * m_sweptRadius;
  
  Quartic<double> quartic(
    dd * dd,
    4.0 * dd * od,
    2.0 * dd * oorr + 4.0 * od * od + fourRR * direction.y() * direction.y(),
    4.0 * od * oorr + 2.0 * fourRR * origin.y() * direction.y(),
    oorr * oorr - fourRR * (m_tubeRadius * m_tubeRadius - origin.y() * origin.y())
  );
  
  vector<double> results = quartic.sortedResult();
  
  for (int i = 0; i != results.size(); ++i) {
    if (results[i] >  0) {
      Vector3d hitPoint = ray.at(results[i]);
      hitPoints.add(HitPoint(results[i], hitPoint, computeNormal(hitPoint)));
      return this;
    }
  }
  return 0;
}

BoundingBox Torus::boundingBox() {
  Vector3d corner(m_sweptRadius + m_tubeRadius, m_tubeRadius, m_sweptRadius + m_tubeRadius);
  return BoundingBox(-corner, corner);
}

Vector3d Torus::computeNormal(const Vector3d& p) const {
  double paramSquared = m_sweptRadius * m_sweptRadius + m_tubeRadius * m_tubeRadius;
  double sumSquared = p * p;
  
  Vector3d result(
    4.0 * p.x() * (sumSquared - paramSquared),
    4.0 * p.y() * (sumSquared - paramSquared + 2.0 * m_sweptRadius * m_sweptRadius),
    4.0 * p.z() * (sumSquared - paramSquared)
  );
  
  return result.normalized();
}
