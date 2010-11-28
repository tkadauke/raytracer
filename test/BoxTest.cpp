#include "gtest/gtest.h"
#include "Box.h"
#include "Ray.h"
#include "HitPointInterval.h"

namespace BoxTest {
  TEST(Box, ShouldInitializeWithValues) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
  }
  
  TEST(Box, ShouldIntersectWithRay) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Ray ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    Surface* surface = box.intersect(ray, hitPoints);
    ASSERT_EQ(surface, &box);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
  }
  
  TEST(Box, ShouldNotIntersectWithMissingRay) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Ray ray(Vector3d(0, 0, -2), Vector3d(0, 1, 0));
    
    HitPointInterval hitPoints;
    Surface* surface = box.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, surface);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Box, ShouldNotIntersectIfBoxIsBehindRayOrigin) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Ray ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    Surface* surface = box.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, surface);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Box, ShouldReportBothHitpointsWhenRayOriginIsInsideBox) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Ray ray(Vector3d(0, 0, 0), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    Surface* surface = box.intersect(ray, hitPoints);
    ASSERT_EQ(surface, &box);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(-1, hitPoints.min().distance());

    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().point());
    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().normal());
    ASSERT_EQ(1, hitPoints.max().distance());
  }
}
