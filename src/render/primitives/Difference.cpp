#include "render/State.h"
#include "render/primitives/Difference.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include <QDebug>

using namespace render;

const Primitive* Difference::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                       render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return nullptr;
  }

  bool firstElement = true;

  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    if (i->intersect(ray, candidate, state)) {
      if (firstElement) {
        hitPoints = candidate;
      } else {
        hitPoints = hitPoints - candidate;
      }
    } else if (firstElement) {
      return nullptr;
    }
    firstElement = false;
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

PrimitivePacketHit4 Difference::intersectPacketHits(const Ray4& rays,
                                                    const PrimitivePacketState4& states) const {
  return Primitive::intersectPacketHits(rays, states);
}

PrimitivePacketHit8 Difference::intersectPacketHits(const Ray8& rays,
                                                    const PrimitivePacketState8& states) const {
  return Primitive::intersectPacketHits(rays, states);
}

// Shadow implementation of Composite, which generates spourious shadows of
// differential objects
bool Difference::intersects(const Rayd& ray, render::State& state) const {
  return Primitive::intersects(ray, state);
}

std::shared_ptr<Mesh> Difference::tessellate(int) const {
  qWarning() << "Difference::tessellate not implemented — CSG mesh booleans not implemented.";
  return std::make_shared<Mesh>();
}

BoundingBoxd Difference::calculateBoundingBox() const {
  if (primitives().size() > 0) {
    return primitives().front()->boundingBox();
  } else {
    return BoundingBoxd();
  }
}
