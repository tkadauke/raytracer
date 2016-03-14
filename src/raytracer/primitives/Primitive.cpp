#include "raytracer/State.h"
#include "raytracer/primitives/Primitive.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

using namespace raytracer;

bool Primitive::intersects(const Rayd& ray) {
  State state; // removeme
  HitPointInterval hitPoints;
  return intersect(ray, hitPoints, state);
}
