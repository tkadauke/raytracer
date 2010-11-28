#include "gtest/gtest.h"
#include "Sphere.h"
#include "Ray.h"
#include "HitPointInterval.h"

namespace SphereTest {
  TEST(Sphere, ShouldInitializeWithValues) {
    Sphere sphere(Vector3d(), 1);
  }
  
  TEST(Sphere, ShouldIntersectWithRay) {
    Sphere sphere(Vector3d(), 1);
    Ray ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    Surface* surface = sphere.intersect(ray, hitPoints);
    ASSERT_EQ(surface, &sphere);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
  }
  
  TEST(Sphere, ShouldNotIntersectWithMissingRay) {
    Sphere sphere(Vector3d(), 1);
    Ray ray(Vector3d(0, 0, -2), Vector3d(0, 1, 0));
    
    HitPointInterval hitPoints;
    Surface* surface = sphere.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, surface);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Sphere, ShouldNotIntersectIfSphereIsBehindRayOrigin) {
    Sphere sphere(Vector3d(), 1);
    Ray ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    Surface* surface = sphere.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, surface);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Sphere, ShouldReportBothHitpointsWhenRayOriginIsInsideSphere) {
    Sphere sphere(Vector3d(), 1);
    Ray ray(Vector3d(0, 0, 0), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    Surface* surface = sphere.intersect(ray, hitPoints);
    ASSERT_EQ(surface, &sphere);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(-1, hitPoints.min().distance());

    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().point());
    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().normal());
    ASSERT_EQ(1, hitPoints.max().distance());
  }
}
