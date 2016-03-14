#include "gtest.h"
#include "raytracer/State.h"
#include "raytracer/primitives/Plane.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

namespace PlaneTest {
  using namespace raytracer;

  TEST(Plane, ShouldInitializeWithValues) {
    Plane plane(Vector3d(0, 1, 0), 0);
  }
  
  TEST(Plane, ShouldIntersectWithRay) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Rayd ray(Vector3d(0, 1, 0), Vector3d(0, -1, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = plane.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &plane);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 1, 0), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }
  
  TEST(Plane, ShouldNotIntersectWithParallelRay) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = plane.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }
  
  TEST(Plane, ShouldNotIntersectIfPointIsBehindRayOrigin) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Rayd ray(Vector3d(0, -1, 0), Vector3d(0, -1, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = plane.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }
  
  TEST(Plane, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Rayd ray(Vector3d(0, 1, 0), Vector3d(0, -1, 0));
    
    ASSERT_TRUE(plane.intersects(ray));
  }
  
  TEST(Plane, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Rayd ray(Vector3d(0, -1, 0), Vector3d(0, -1, 0));
    
    ASSERT_FALSE(plane.intersects(ray));
  }
  
  TEST(Plane, ShouldReturnBoundingBox) {
    // TODO
  }
}
