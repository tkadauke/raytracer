#include "gtest/gtest.h"
#include "surfaces/Disk.h"
#include "math/Ray.h"
#include "math/HitPointInterval.h"

namespace DiskTest {
  TEST(Disk, ShouldInitializeWithValues) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
  }
  
  TEST(Disk, ShouldIntersectWithRay) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    Ray ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    Surface* surface = disk.intersect(ray, hitPoints);
    ASSERT_EQ(surface, &disk);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(2, hitPoints.min().distance());
  }
  
  TEST(Disk, ShouldNotIntersectWithMissingRay) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    Ray ray(Vector3d(0, 0, -2), Vector3d(0, 1, 0));
    
    HitPointInterval hitPoints;
    Surface* surface = disk.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, surface);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Disk, ShouldNotIntersectIfDiskIsBehindRayOrigin) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    Ray ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    Surface* surface = disk.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, surface);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Disk, ShouldReturnBoundingDisk) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    BoundingBox bbox = disk.boundingBox();
    ASSERT_EQ(Vector3d(-1, -1, -1), bbox.min());
    ASSERT_EQ(Vector3d(1, 1, 1), bbox.max());
  }
}
