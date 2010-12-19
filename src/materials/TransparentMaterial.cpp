#include "materials/TransparentMaterial.h"
#include "Raytracer.h"
#include "math/HitPoint.h"
#include "math/Ray.h"

#include <algorithm>

using namespace std;

Colord TransparentMaterial::shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth) {
  Colord color = PhongMaterial::shade(raytracer, ray, hitPoint, recursionDepth);

  Vector3d reflectionDirection = ray.direction() - hitPoint.normal() * (2.0 * (ray.direction() * hitPoint.normal()));

  Vector3d refractionDirection;
  double angle;
  Colord transparency;
  
  if (ray.direction() * hitPoint.normal() < 0) {
    refractionDirection = refract(ray.direction(), hitPoint.normal(), 1.0, m_refractionIndex);
    angle = -ray.direction() * hitPoint.normal();
    transparency = Colord::white;
  } else {
    double distance = hitPoint.distance();
    Colord absorbance = m_absorbanceColor * -distance * 0.15;
    transparency = Colord(expf(absorbance.r()), expf(absorbance.g()), expf(absorbance.b()));
    
    refractionDirection = refract(ray.direction(), -hitPoint.normal(), m_refractionIndex, 1.0);
    
    if (refractionDirection.isUndefined()) {
      return color + raytracer->rayColor(Ray(hitPoint.point(), reflectionDirection).epsilonShifted(), recursionDepth + 1) * transparency;
    } else {
      angle = refractionDirection * hitPoint.normal();
    }
  }
  
  double R0 = (m_refractionIndex - 1.0) * (m_refractionIndex - 1.0) / (m_refractionIndex + 1.0) * (m_refractionIndex + 1.0);
  double R = R0 + (1.0 - R0) * pow(1.0 - angle, 5.0);
  
  color = color + transparency * (raytracer->rayColor(Ray(hitPoint.point(), reflectionDirection).epsilonShifted(), recursionDepth + 1) * R +
                                  raytracer->rayColor(Ray(hitPoint.point(), refractionDirection).epsilonShifted(), recursionDepth + 1) * (1.0 - R));
  
  return color;
}

Vector3d TransparentMaterial::refract(const Vector3d& direction, const Vector3d& normal, double outerRefractionIndex, double innerRefractionIndex) {
  double cosTheta = direction * normal;
  double refractionIndex = outerRefractionIndex / innerRefractionIndex;
  double cosPhiSquared = 1.0 - (refractionIndex * refractionIndex * (1.0 - cosTheta * cosTheta));
  
  if (cosPhiSquared < 0.0) {
    return Vector3d::undefined;
  } else {
    return (direction * refractionIndex) + normal * (refractionIndex * -cosTheta - sqrt(cosPhiSquared));
  }
}
