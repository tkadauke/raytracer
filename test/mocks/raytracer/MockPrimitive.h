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

    MOCK_CONST_METHOD3(intersect, const Primitive*(const Rayd&, HitPointInterval&, raytracer::State&));
    MOCK_CONST_METHOD2(intersects, bool(const Rayd&, raytracer::State&));
    MOCK_CONST_METHOD1(farthestPoint, Vector3d(const Vector3d&));
    MOCK_CONST_METHOD0(calculateBoundingBox, BoundingBoxd());
    MOCK_METHOD0(destructorCall, void());
    
    inline bool defaultIntersects(const Rayd& ray, raytracer::State& state) const {
      return Primitive::intersects(ray, state);
    }

    inline Vector3d defaultFarthestPoint(const Vector3d& direction) const {
      return Primitive::farthestPoint(direction);
    }
  };

  ACTION_P(AddHitPoint, hitPoint) {
    arg1.add(hitPoint);
  }

  ACTION_P2(AddHitPoints, hitPoint1, hitPoint2) {
    arg1.add(hitPoint1, hitPoint2);
  }
}

#endif
