#ifndef MOCK_SURFACE_H
#define MOCK_SURFACE_H

#include "gmock/gmock.h"
#include "surfaces/Surface.h"
#include "math/Ray.h"
#include "math/HitPointInterval.h"

namespace testing {
  class MockSurface : public Surface {
  public:
    virtual ~MockSurface() { destructorCall(); }

    MOCK_METHOD2(intersect, Surface*(const Ray&, HitPointInterval&));
    MOCK_METHOD1(intersects, bool(const Ray&));
    MOCK_METHOD0(boundingBox, BoundingBox());
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
