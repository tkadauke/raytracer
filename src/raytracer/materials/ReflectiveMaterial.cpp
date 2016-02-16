#include "raytracer/materials/ReflectiveMaterial.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

using namespace raytracer;

Colord ReflectiveMaterial::shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth) {
  auto color = PhongMaterial::shade(raytracer, ray, hitPoint, recursionDepth);

  Vector3d reflectionDirection = ray.direction() - hitPoint.normal() * (2.0 * (ray.direction() * hitPoint.normal()));

  // TODO: fix reflection in a later commit
  // color += specularColor() * raytracer->rayColor(Ray(hitPoint.point(), reflectionDirection).epsilonShifted(), recursionDepth + 1);
  
  return color;
}
