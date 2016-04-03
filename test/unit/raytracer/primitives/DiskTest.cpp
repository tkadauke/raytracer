#include "gtest/gtest.h"
#include "raytracer/State.h"
#include "raytracer/primitives/Disk.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"
#include "test/helpers/VectorTestHelper.h"

namespace DiskTest {
  using namespace raytracer;

  TEST(Disk, ShouldInitializeWithValues) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
  }
  
  TEST(Disk, ShouldIntersectWithRay) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = disk.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &disk);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(2, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }
  
  TEST(Disk, ShouldNotIntersectWithMissingRay) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 1, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = disk.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }
  
  TEST(Disk, ShouldNotIntersectIfDiskIsBehindRayOrigin) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    Rayd ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = disk.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, primitive);
    ASSERT_FALSE(hitPoints.min().isUndefined());
    ASSERT_TRUE(hitPoints.minWithPositiveDistance().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }
  
  TEST(Disk, ShouldReturnFarthestPoint) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    auto direction = Vector3d(1, 1, 1).normalized();
    auto expected = Vector3d(1, 1, 0).normalized();
    
    ASSERT_VECTOR_NEAR(expected, disk.farthestPoint(direction), 0.001);
  }
  
  TEST(Disk, ShouldReturnBoundingBox) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    BoundingBoxd bbox = disk.boundingBox();
    ASSERT_EQ(Vector3d(-1, -1, -1), bbox.min());
    ASSERT_EQ(Vector3d(1, 1, 1), bbox.max());
  }
}
