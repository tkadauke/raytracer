#include "gtest/gtest.h"
#include "raytracer/State.h"
#include "raytracer/primitives/Rectangle.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

namespace RectangleTest {
  using namespace raytracer;

  TEST(Rectangle, ShouldInitializeWithValues) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
  }
  
  TEST(Rectangle, ShouldInitializeWithNormal) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0), Vector3d(0, 0, 1));
  }
  
  TEST(Rectangle, ShouldIntersectWithRay) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = rectangle.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &rectangle);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.min().normal());
    ASSERT_EQ(2, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }
  
  TEST(Rectangle, ShouldNotIntersectWithParallelRay) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    Rayd ray(Vector3d(0, 0, -2), Vector3d(1, 1, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = rectangle.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }
  
  TEST(Rectangle, ShouldNotIntersectWithMissingRay) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    Rayd ray(Vector3d(-2, -2, -2), Vector3d(0, 0, 1));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = rectangle.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }
  
  TEST(Rectangle, ShouldNotIntersectIfRectangleIsBehindRayOrigin) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    Rayd ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = rectangle.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.minWithPositiveDistance().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }
  
  TEST(Rectangle, ShouldReturnBoundingBox) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    BoundingBoxd expected = BoundingBoxd(Vector3d(-1, -1, 0), Vector3d(0, 0, 0)).grownByEpsilon();

    ASSERT_EQ(expected, rectangle.boundingBox());
  }
}
