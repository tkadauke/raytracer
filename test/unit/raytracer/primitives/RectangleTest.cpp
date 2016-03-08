#include "gtest/gtest.h"
#include "raytracer/primitives/Rectangle.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

namespace RectangleTest {
  using namespace raytracer;

  TEST(Rectangle, ShouldInitializeWithValues) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
  }
  
  TEST(Rectangle, ShouldIntersectWithRay) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    auto primitive = rectangle.intersect(ray, hitPoints);
    ASSERT_EQ(primitive, &rectangle);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.min().normal());
    ASSERT_EQ(2, hitPoints.min().distance());
  }
  
  TEST(Rectangle, ShouldNotIntersectWithParallelRay) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    Rayd ray(Vector3d(0, 0, -2), Vector3d(1, 1, 0));
    
    HitPointInterval hitPoints;
    auto primitive = rectangle.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Rectangle, ShouldNotIntersectWithMissingRay) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    Rayd ray(Vector3d(-2, -2, -2), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    auto primitive = rectangle.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Rectangle, ShouldNotIntersectIfRectangleIsBehindRayOrigin) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    Rayd ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    auto primitive = rectangle.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Rectangle, ShouldReturnBoundingBox) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    BoundingBoxd bbox = rectangle.boundingBox();
    ASSERT_EQ(Vector3d(-1, -1, 0), bbox.min());
    ASSERT_EQ(Vector3d(0, 0, 0), bbox.max());
  }
}
