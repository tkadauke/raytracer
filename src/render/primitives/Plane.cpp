#include "render/State.h"
#include "render/primitives/Plane.h"
#include "core/geometry/Mesh.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"
#include <QDebug>

using namespace render;

const Primitive* Plane::intersect(const Rayd& ray, HitPointInterval& hitPoints, render::State& state) const {
  double t = calculateIntersectionDistance(ray);
  
  if (t > 0) {
    hitPoints.add(HitPoint(this, t, ray.at(t), m_normal));
    state.hit(this, "Plane");
    return this;
  } else {
    state.miss(this, "Plane, ray miss");
    return nullptr;
  }
}

bool Plane::intersects(const Rayd& ray, render::State& state) const {
  if (calculateIntersectionDistance(ray) > 0) {
    state.shadowHit(this, "Plane");
    return true;
  }
  
  state.shadowMiss(this, "Plane");
  return false;
}

double Plane::calculateIntersectionDistance(const Rayd& ray) const {
  const Vector3d& o = ray.origin(), d = ray.direction();
  
  double angle = m_normal * d;
  if (angle == 0)
    return false;
  
  return -(m_normal * o + m_distance) / angle;
}

std::shared_ptr<Mesh> Plane::tessellate(int) const {
  qWarning() << "Plane is infinite; tessellate() returns empty mesh — clip to a finite region first.";
  return std::make_shared<Mesh>();
}

BoundingBoxd Plane::calculateBoundingBox() const {
  return BoundingBoxd::infinity();
}
