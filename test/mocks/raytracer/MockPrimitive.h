#ifndef MOCK_PRIMITIVE_H
#define MOCK_PRIMITIVE_H

#include "gmock/gmock.h"
#include "raytracer/State.h"
#include "raytracer/primitives/Primitive.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

namespace testing {
  class MockPrimitive : public raytracer::Primitive {
  public:
    inline virtual ~MockPrimitive() {
      destructorCall();
    }

    MOCK_METHOD3(intersect, Primitive*(const Rayd&, HitPointInterval&, raytracer::State&));
    MOCK_METHOD2(intersects, bool(const Rayd&, raytracer::State&));
    MOCK_METHOD0(boundingBox, BoundingBoxd());
    MOCK_METHOD0(destructorCall, void());
  };

  ACTION_P(AddHitPoint, hitPoint) {
    arg1.add(hitPoint);
  }

  ACTION_P2(AddHitPoints, hitPoint1, hitPoint2) {
    arg1.add(hitPoint1, hitPoint2);
  }
}

#endif
