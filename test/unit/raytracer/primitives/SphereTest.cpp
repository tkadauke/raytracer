#include "gtest/gtest.h"
#include "raytracer/State.h"
#include "raytracer/primitives/Sphere.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"
#include "test/helpers/VectorTestHelper.h"

namespace SphereTest {
  using namespace raytracer;
  
  TEST(Sphere, ShouldInitializeWithValues) {
    Sphere sphere(Vector3d(), 1);
  }
  
  TEST(Sphere, ShouldIntersectWithRay) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = sphere.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &sphere);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(2u, hitPoints.points().size());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }
  
  TEST(Sphere, ShouldNotIntersectWithMissingRay) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 1, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = sphere.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }
  
  TEST(Sphere, ShouldNotIntersectIfSphereIsBehindRayOrigin) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = sphere.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.minWithPositiveDistance().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }
  
  TEST(Sphere, ShouldReportBothHitpointsWhenRayOriginIsInsideSphere) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(0, 0, 0), Vector3d(0, 0, 1));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = sphere.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &sphere);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(-1, hitPoints.min().distance());

    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().point());
    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().normal());
    ASSERT_EQ(1, hitPoints.max().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }
  
  TEST(Sphere, ShouldReturnTrueForIntersectsIfThereIsAIntersectionWithRay) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));
    
    State state;
    ASSERT_TRUE(sphere.intersects(ray, state));
  }
  
  TEST(Sphere, ShouldReturnFalseForIntersectsWithMissingRay) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 1, 0));
    
    State state;
    ASSERT_FALSE(sphere.intersects(ray, state));
  }
  
  TEST(Sphere, ShouldReturnFalseForIntersectsIfSphereIsBehindRayOrigin) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));
    
    State state;
    ASSERT_FALSE(sphere.intersects(ray, state));
  }

  TEST(Sphere, ShouldReturnTrueForIntersectsIfRayIsInsideSphere) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(), Vector3d(0, 0, 1));
    
    State state;
    ASSERT_TRUE(sphere.intersects(ray, state));
  }
  
  TEST(Sphere, ShouldReturnFarthestPoint) {
    Sphere sphere(Vector3d(), 1);
    auto direction = Vector3d(0.1, 1, 0.1).normalized();
    ASSERT_VECTOR_NEAR(direction, sphere.farthestPoint(direction), 0.001);
  }
  
  TEST(Sphere, ShouldReturnBoundingBox) {
    Sphere sphere(Vector3d(), 1);
    ASSERT_EQ(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1)), sphere.boundingBox());
  }
}
