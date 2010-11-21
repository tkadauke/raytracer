#include "Sphere.h"
#include "Ray.h"
#include "HitPointInterval.h"
#include <cmath>

using namespace std;

Surface* Sphere::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  const Vector3d& o = ray.origin() - m_origin, d = ray.direction();
  
  double od = o * d, dd = d * d;
  double discriminant = od * od - dd * (o * o - m_radius * m_radius);
  
  if (discriminant < 0) {
    return 0;
  } else if (discriminant > 0) {
    double discriminantRoot = sqrt(discriminant);
    double t1 = (-od - discriminantRoot) / dd;
    double t2 = (-od + discriminantRoot) / dd;
    if (t1 <= 0 && t2 <= 0)
      return 0;
    
    // double t;
    // if (t1 <= 0)
    //   t = t2;
    // else if (t2 <= 0)
    //   t = t1;
    // else
    //   t = min(t1, t2);
    
    Vector3d hitPoint1 = ray.at(t1),
             hitPoint2 = ray.at(t2);
    hitPoints.add(HitPoint(t1, hitPoint1, (hitPoint1 - m_origin) / m_radius),
                      HitPoint(t2, hitPoint2, (hitPoint2 - m_origin) / m_radius));
    return this;
  }
  return 0;
}
