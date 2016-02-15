#include "raytracer/materials/TransparentMaterial.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

#include <algorithm>

using namespace std;
using namespace raytracer;

Colord TransparentMaterial::shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth) {
  auto color = PhongMaterial::shade(raytracer, ray, hitPoint, recursionDepth);

  Vector3d reflectionDirection = Vector3d::undefined();

  Vector3d refractionDirection;
  double angle;
  Colord transparency;
  
  if (ray.direction() * hitPoint.normal() < 0) {
    refractionDirection = refract(ray.direction(), hitPoint.normal(), 1.0, m_refractionIndex);
    reflectionDirection = ray.direction() - hitPoint.normal() * (2.0 * (ray.direction() * hitPoint.normal()));
    angle = -ray.direction() * hitPoint.normal();
    transparency = Colord::white();
  } else {
    refractionDirection = refract(ray.direction(), -hitPoint.normal(), m_refractionIndex, 1.0);
    reflectionDirection = ray.direction() + hitPoint.normal() * (2.0 * (ray.direction() * -hitPoint.normal()));
    
    if (refractionDirection.isUndefined()) {
      return raytracer->rayColor(Ray(hitPoint.point(), reflectionDirection.normalized()).epsilonShifted(), recursionDepth + 1);
    } else {
      angle = refractionDirection * hitPoint.normal();
    }

    double distance = hitPoint.distance();
    auto absorbance = m_absorbanceColor * -distance * 0.15;
    transparency = Colord(expf(absorbance.r()), expf(absorbance.g()), expf(absorbance.b()));
  }
  
  double R0 = (m_refractionIndex - 1.0) * (m_refractionIndex - 1.0) / (m_refractionIndex + 1.0) * (m_refractionIndex + 1.0);
  double R = R0 + (1.0 - R0) * pow(1.0 - angle, 5.0);
  
  color = color + highlightColor() * (raytracer->rayColor(Ray(hitPoint.point(), reflectionDirection.normalized()).epsilonShifted(), recursionDepth + 1) * R);
  color = color + transparency * (raytracer->rayColor(Ray(hitPoint.point(), refractionDirection.normalized()).epsilonShifted(), recursionDepth + 1) * (1.0 - R));
  
  return color;
}

Vector3d TransparentMaterial::refract(const Vector3d& direction, const Vector3d& normal, double outerRefractionIndex, double innerRefractionIndex) {
  double cosTheta = direction * normal;
  double refractionIndex = outerRefractionIndex / innerRefractionIndex;
  double cosPhiSquared = 1.0 - (refractionIndex * refractionIndex * (1.0 - cosTheta * cosTheta));
  
  if (cosPhiSquared < 0.0) {
    return Vector3d::undefined();
  } else {
    return (direction * refractionIndex) + normal * (refractionIndex * -cosTheta - sqrt(cosPhiSquared));
  }
}

bool TransparentMaterial::totalInternalReflection(const Ray& ray, const HitPoint& hitPoint) {
  Vector3d wo = -ray.direction();
  float cosTheta = hitPoint.normal() * wo;
  float eta = m_refractionIndex;
  
  if (cosTheta < 0)
    eta = 1.0 / eta;
  
  return (1.0 - (1.0 - cosTheta * cosTheta) / (eta * eta) < 0.0);
}
