#include "gtest/gtest.h"
#include "raytracer/primitives/Box.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

namespace BoxTest {
  using namespace raytracer;

  TEST(Box, ShouldInitializeWithValues) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
  }
  
  TEST(Box, ShouldIntersectWithRayInXDirection) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Ray ray(Vector3d(-2, 0, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    auto primitive = box.intersect(ray, hitPoints);
    ASSERT_EQ(primitive, &box);
    ASSERT_EQ(Vector3d(-1, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(-1, 0, 0), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
  }
  
  TEST(Box, ShouldIntersectWithRayInYDirection) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Ray ray(Vector3d(0, -2, 0), Vector3d(0, 1, 0));
    
    HitPointInterval hitPoints;
    auto primitive = box.intersect(ray, hitPoints);
    ASSERT_EQ(primitive, &box);
    ASSERT_EQ(Vector3d(0, -1, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, -1, 0), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
  }
  
  TEST(Box, ShouldIntersectWithRayInZDirection) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Ray ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    auto primitive = box.intersect(ray, hitPoints);
    ASSERT_EQ(primitive, &box);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
  }
  
  TEST(Box, ShouldIntersectIfRayIsTangentToPrimitive) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Ray ray(Vector3d(0, 1, -2), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    auto primitive = box.intersect(ray, hitPoints);
    ASSERT_EQ(primitive, &box);
    ASSERT_EQ(Vector3d(0, 1, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
  }
  
  TEST(Box, ShouldNotIntersectWithMissingRay) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Ray ray(Vector3d(0, 0, -2), Vector3d(0, 1, 0));
    
    HitPointInterval hitPoints;
    auto primitive = box.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Box, ShouldNotIntersectIfBoxIsBehindRayOrigin) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Ray ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    auto primitive = box.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Box, ShouldReportBothHitpointsWhenRayOriginIsInsideBox) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Ray ray(Vector3d(0, 0, 0), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    auto primitive = box.intersect(ray, hitPoints);
    ASSERT_EQ(primitive, &box);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(-1, hitPoints.min().distance());

    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().point());
    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().normal());
    ASSERT_EQ(1, hitPoints.max().distance());
  }
  
  TEST(Box, ShouldReturnBoundingBox) {
    Box box(Vector3d::null(), Vector3d(1, 1, 1));
    BoundingBoxd bbox = box.boundingBox();
    ASSERT_EQ(Vector3d(-1, -1, -1), bbox.min());
    ASSERT_EQ(Vector3d(1, 1, 1), bbox.max());
  }
}
