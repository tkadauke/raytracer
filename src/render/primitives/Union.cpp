#include "render/State.h"
#include "render/primitives/Union.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include <QDebug>

using namespace render;

const Primitive* Union::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                  render::State& state) const {
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

PrimitivePacketHit4 Union::intersectPacketHits(const Ray4& rays,
                                               const PrimitivePacketState4& states) const {
  return Primitive::intersectPacketHits(rays, states);
}

PrimitivePacketHit8 Union::intersectPacketHits(const Ray8& rays,
                                               const PrimitivePacketState8& states) const {
  return Primitive::intersectPacketHits(rays, states);
}

std::shared_ptr<Mesh> Union::tessellate(int) const {
  qWarning() << "Union::tessellate not implemented — CSG mesh booleans not implemented.";
  return std::make_shared<Mesh>();
}

bool Union::intersects(const Rayd& ray, render::State& state) const {
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
