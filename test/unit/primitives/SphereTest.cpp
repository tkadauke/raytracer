#include "gtest/gtest.h"
#include "primitives/Sphere.h"
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
    Primitive* primitive = sphere.intersect(ray, hitPoints);
    ASSERT_EQ(primitive, &sphere);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(2u, hitPoints.points().size());
    ASSERT_EQ(1, hitPoints.min().distance());
  }
  
  TEST(Sphere, ShouldNotIntersectWithMissingRay) {
    Sphere sphere(Vector3d(), 1);
    Ray ray(Vector3d(0, 0, -2), Vector3d(0, 1, 0));
    
    HitPointInterval hitPoints;
    Primitive* primitive = sphere.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Sphere, ShouldNotIntersectIfSphereIsBehindRayOrigin) {
    Sphere sphere(Vector3d(), 1);
    Ray ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    Primitive* primitive = sphere.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Sphere, ShouldReportBothHitpointsWhenRayOriginIsInsideSphere) {
    Sphere sphere(Vector3d(), 1);
    Ray ray(Vector3d(0, 0, 0), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    Primitive* primitive = sphere.intersect(ray, hitPoints);
    ASSERT_EQ(primitive, &sphere);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(-1, hitPoints.min().distance());

    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().point());
    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().normal());
    ASSERT_EQ(1, hitPoints.max().distance());
  }
  
  TEST(Sphere, ShouldReturnTrueForIntersectsIfThereIsAIntersectionWithRay) {
    Sphere sphere(Vector3d(), 1);
    Ray ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));
    
    ASSERT_TRUE(sphere.intersects(ray));
  }
  
  TEST(Sphere, ShouldReturnFalseForIntersectsWithMissingRay) {
    Sphere sphere(Vector3d(), 1);
    Ray ray(Vector3d(0, 0, -2), Vector3d(0, 1, 0));
    
    ASSERT_FALSE(sphere.intersects(ray));
  }
  
  TEST(Sphere, ShouldReturnFalseForIntersectsIfSphereIsBehindRayOrigin) {
    Sphere sphere(Vector3d(), 1);
    Ray ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));
    
    ASSERT_FALSE(sphere.intersects(ray));
  }

  TEST(Sphere, ShouldReturnTrueForIntersectsIfRayIsInsideSphere) {
    Sphere sphere(Vector3d(), 1);
    Ray ray(Vector3d(), Vector3d(0, 0, 1));
    
    ASSERT_TRUE(sphere.intersects(ray));
  }
  
  TEST(Sphere, ShouldReturnBoundingBox) {
    Sphere sphere(Vector3d(), 1);
    ASSERT_EQ(BoundingBox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1)), sphere.boundingBox());
  }
}
