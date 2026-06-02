#include "render/State.h"
#include "render/primitives/Intersection.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include <QDebug>

using namespace render;

const Primitive* Intersection::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                         render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return nullptr;
  }

  unsigned int numHits = 0;
  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    if (i->intersect(ray, candidate, state)) {
      if (numHits) {
        hitPoints = hitPoints & candidate;
      } else {
        hitPoints = candidate;
      }
      numHits++;
    }
  }

  if (numHits != primitives().size() || hitPoints.empty()) {
    return nullptr;
  } else {
    if (material()) {
      hitPoints.setPrimitive(this);
      return this;
    } else {
      return hitPoints.minWithPositiveDistance().primitive();
    }
  }
}

PrimitivePacketHit4 Intersection::intersectPacketHits(const Ray4& rays,
                                                      const PrimitivePacketState4& states) const {
  return Primitive::intersectPacketHits(rays, states);
}

PrimitivePacketHit8 Intersection::intersectPacketHits(const Ray8& rays,
                                                      const PrimitivePacketState8& states) const {
  return Primitive::intersectPacketHits(rays, states);
}

bool Intersection::intersects(const Rayd& ray, render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return false;
  }

  for (const auto& i : primitives()) {
    if (!i->intersects(ray, state))
      return false;
  }

  return true;
}

std::shared_ptr<Mesh> Intersection::tessellate(int) const {
  qWarning() << "Intersection::tessellate not implemented — CSG mesh booleans not implemented.";
  return std::make_shared<Mesh>();
}

BoundingBoxd Intersection::calculateBoundingBox() const {
  BoundingBoxd result;
  int num = 0;
  for (const auto& i : primitives()) {
    if (num++ == 0)
      result = i->boundingBox();
    else
      result &= i->boundingBox();
  }
  return result;
}
