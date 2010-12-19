#include "gtest/gtest.h"
#include "surfaces/Sphere.h"
#include "math/Ray.h"
#include "math/HitPointInterval.h"

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
  
  TEST(Sphere, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    Sphere sphere(Vector3d(), 1);
    Ray ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));
    
    ASSERT_TRUE(sphere.intersects(ray));
  }
  
  TEST(Sphere, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Sphere sphere(Vector3d(), 1);
    Ray ray(Vector3d(0, 0, -2), Vector3d(0, 1, 0));
    
    ASSERT_FALSE(sphere.intersects(ray));
  }
}
