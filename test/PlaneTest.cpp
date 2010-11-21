#include "gtest.h"
#include "Plane.h"
#include "Ray.h"
#include "HitPointInterval.h"

namespace PlaneTest {
  TEST(Plane, ShouldInitializeWithValues) {
    Plane plane(Vector3d(0, 1, 0), 0);
  }
  
  TEST(Plane, ShouldIntersectWithRay) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Ray ray(Vector3d(0, 1, 0), Vector3d(0, -1, 0));
    
    HitPointInterval hitPoints;
    Surface* surface = plane.intersect(ray, hitPoints);
    ASSERT_EQ(surface, &plane);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 1, 0), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
  }
  
  TEST(Plane, ShouldNotIntersectWithParallelRay) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* surface = plane.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, surface);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Plane, ShouldNotIntersectIfPointIsBehindRayOrigin) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Ray ray(Vector3d(0, -1, 0), Vector3d(0, -1, 0));
    
    HitPointInterval hitPoints;
    Surface* surface = plane.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, surface);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
}
