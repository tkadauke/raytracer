#include "gtest/gtest.h"
#include "raytracer/primitives/Torus.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

namespace TorusTest {
  using namespace raytracer;

  TEST(Torus, ShouldInitializeWithValues) {
    Torus torus(2, 1);
  }
  
  TEST(Torus, ShouldIntersectWithRay) {
    Torus torus(2, 1);
    Ray ray(Vector3d(0, 0, -4), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    auto primitive = torus.intersect(ray, hitPoints);
    ASSERT_EQ(primitive, &torus);
    ASSERT_EQ(Vector3d(0, 0, -3), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(4u, hitPoints.points().size());
    ASSERT_EQ(1, hitPoints.min().distance());
  }
  
  TEST(Torus, ShouldNotIntersectWithMissingRay) {
    Torus torus(2, 1);
    Ray ray(Vector3d(0, 0, -4), Vector3d(0, 1, 0));
    
    HitPointInterval hitPoints;
    auto primitive = torus.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Torus, ShouldNotIntersectIfTorusIsBehindRayOrigin) {
    Torus torus(2, 1);
    Ray ray(Vector3d(0, 0, 4), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    auto primitive = torus.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Torus, ShouldNotIntersectWithTorusHole) {
    Torus torus(2, 1);
    Ray ray(Vector3d(0, 4, 0), Vector3d(0, -1, 0));
    
    HitPointInterval hitPoints;
    auto primitive = torus.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST(Torus, ShouldReturnTrueForIntersectsIfThereIsAIntersectionWithRay) {
    Torus torus(2, 1);
    Ray ray(Vector3d(0, 0, -4), Vector3d(0, 0, 1));
    
    ASSERT_TRUE(torus.intersects(ray));
  }
  
  TEST(Torus, ShouldReturnFalseForIntersectsWithMissingRay) {
    Torus torus(2, 1);
    Ray ray(Vector3d(0, 0, -4), Vector3d(0, 1, 0));
    
    ASSERT_FALSE(torus.intersects(ray));
  }
  
  TEST(Torus, ShouldReturnFalseForIntersectsIfTorusIsBehindRayOrigin) {
    Torus torus(2, 1);
    Ray ray(Vector3d(0, 0, 4), Vector3d(0, 0, 1));
    
    ASSERT_FALSE(torus.intersects(ray));
  }

  TEST(Torus, ShouldReturnTrueForIntersectsIfRayIsInsideTorus) {
    Torus torus(2, 1);
    Ray ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));
    
    ASSERT_TRUE(torus.intersects(ray));
  }
  
  TEST(Torus, ShouldReturnTrueForIntersectsIfRayIsInsideTorusHole) {
    Torus torus(2, 1);
    Ray ray(Vector3d(), Vector3d(0, 0, 1));
    
    ASSERT_TRUE(torus.intersects(ray));
  }
  
  TEST(Torus, ShouldReturnBoundingBox) {
    Torus torus(2, 1);
    ASSERT_EQ(BoundingBoxd(Vector3d(-3, -1, -3), Vector3d(3, 1, 3)), torus.boundingBox());
  }
}
