#include "ReflectiveMaterial.h"
#include "Raytracer.h"
#include "HitPoint.h"
#include "Ray.h"

Colord ReflectiveMaterial::shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth) {
  Colord color = PhongMaterial::shade(raytracer, ray, hitPoint, recursionDepth);

  Vector3d reflectionDirection = ray.direction() - hitPoint.normal() * (2.0 * (ray.direction() * hitPoint.normal()));

  color = color + m_specularColor * raytracer->rayColor(Ray(hitPoint.point(), reflectionDirection).epsilonShifted(), recursionDepth + 1);
  
  return color;
}
