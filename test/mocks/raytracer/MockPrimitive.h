#pragma once

#include "gmock/gmock.h"
#include "raytracer/State.h"
#include "render/primitives/Primitive.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"
#include "test/mocks/MockDestructor.h"

namespace testing {
  class MockPrimitive : public render::Primitive, public MockDestructor {
  public:
    MOCK_METHOD(const Primitive*, intersect,
                (const Rayd&, HitPointInterval&, raytracer::State&), (const, override));
    MOCK_METHOD(bool, intersects, (const Rayd&, raytracer::State&), (const, override));
    MOCK_METHOD(Vector3d, farthestPoint, (const Vector3d&), (const, override));
    MOCK_METHOD(BoundingBoxd, calculateBoundingBox, (), (const, override));
    
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
