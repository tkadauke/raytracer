#include "gtest.h"
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
    Ray ray(Vector3d(0, 1, 0), Vector3d(0, -1, 0));
    
    HitPointInterval hitPoints;
    Primitive* primitive = plane.intersect(ray, hitPoints);
    ASSERT_EQ(primitive, &plane);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 1, 0), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
  }
  
  TEST(Plane, ShouldNotIntersectWithParallelRay) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Primitive* primitive = plane.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Plane, ShouldNotIntersectIfPointIsBehindRayOrigin) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Ray ray(Vector3d(0, -1, 0), Vector3d(0, -1, 0));
    
    HitPointInterval hitPoints;
    Primitive* primitive = plane.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Plane, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Ray ray(Vector3d(0, 1, 0), Vector3d(0, -1, 0));
    
    ASSERT_TRUE(plane.intersects(ray));
  }
  
  TEST(Plane, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Ray ray(Vector3d(0, -1, 0), Vector3d(0, -1, 0));
    
    ASSERT_FALSE(plane.intersects(ray));
  }
  
  TEST(Plane, ShouldReturnBoundingBox) {
    // TODO
  }
}
