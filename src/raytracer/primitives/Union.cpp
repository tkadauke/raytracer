#include "raytracer/State.h"
#include "raytracer/primitives/Union.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include <QDebug>

using namespace raytracer;

const Primitive* Union::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return nullptr;
  }
  
  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    if (i->intersect(ray, candidate, state)) {
      hitPoints = hitPoints | candidate;
    }
  }
  
  auto hitPoint = hitPoints.minWithPositiveDistance();
  if (hitPoint.isUndefined()) {
    return nullptr;
  } else {
    if (material()) {
      hitPoints.setPrimitive(this);
      return this;
    } else {
      return hitPoint.primitive();
    }
  }
}

std::shared_ptr<Mesh> Union::tessellate(int) const {
  qWarning() << "Union::tessellate not implemented — CSG mesh booleans queued under roadmap §4.2.a.";
  return std::make_shared<Mesh>();
}

bool Union::intersects(const Rayd& ray, State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return false;
  }

  for (const auto& i : primitives()) {
    if (i->intersects(ray, state)) {
      return true;
    }
  }
  return false;
}
