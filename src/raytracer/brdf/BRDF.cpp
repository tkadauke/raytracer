#include "raytracer/brdf/BRDF.h"

using namespace raytracer;

Colord BRDF::calculate(const HitPoint&, const Vector3d&, const Vector3d&) {
  return Colord::black();
}

Colord BRDF::sample(const HitPoint&, const Vector3d&, Vector3d&) {
  return Colord::black();
}

Colord BRDF::reflectance(const HitPoint&, const Vector3d&) {
  return Colord::black();
}
