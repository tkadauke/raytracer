#include "raytracer/brdf/Lambertian.h"
#include "core/math/Constants.h"

using namespace raytracer;

Colord Lambertian::calculate(const HitPoint&, const Vector3d&, const Vector3d&) {
  return diffuseColor() * reflectionCoefficient() * invPI;
}

Colord Lambertian::reflectance(const HitPoint&, const Vector3d&) {
  return diffuseColor() * reflectionCoefficient();
}
