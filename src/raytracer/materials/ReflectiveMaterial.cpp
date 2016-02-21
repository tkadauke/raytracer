#include "raytracer/materials/ReflectiveMaterial.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

using namespace raytracer;

Colord ReflectiveMaterial::shade(std::shared_ptr<Raytracer> raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth) {
  auto color = PhongMaterial::shade(raytracer, ray, hitPoint, recursionDepth);

  Vector3d out = - ray.direction();
  Vector3d in;
  Colord refl = m_reflectiveBRDF.sample(hitPoint, out, in);
  Ray reflected(hitPoint.point(), in);
  
  double normalDotIn = hitPoint.normal() * in;
  
  color += refl * raytracer->rayColor(reflected.epsilonShifted(), recursionDepth + 1) * normalDotIn;

  return color;
}
