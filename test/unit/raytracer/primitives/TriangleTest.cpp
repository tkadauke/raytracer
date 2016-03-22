#include "gtest.h"
#include "raytracer/State.h"
#include "raytracer/primitives/Triangle.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

namespace TriangleTest {
  using namespace ::testing;
  using namespace raytracer;
  
  struct TriangleTest : public ::testing::Test {
    void SetUp() {
      point0 = Vector3d(-1, -1, 0);
      point1 = Vector3d(-1, 1, 0);
      point2 = Vector3d(1, -1, 0);
    }
    
    Vector3d point0, point1, point2;
  };

  TEST_F(TriangleTest, ShouldInitializeWithValues) {
    Triangle triangle(this->point0, this->point1, this->point2);
  }
  
  TEST_F(TriangleTest, ShouldIntersectWithRay) {
    Triangle triangle(this->point0, this->point1, this->point2);
    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = triangle.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &triangle);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }
  
  TEST_F(TriangleTest, ShouldNotIntersectWithMissingRay) {
    Triangle triangle(this->point0, this->point1, this->point2);
    Rayd ray(Vector3d(0, 4, -1), Vector3d(0, 0, 1));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = triangle.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }
  
  TEST_F(TriangleTest, ShouldNotIntersectIfPointIsBehindRayOrigin) {
    Triangle triangle(this->point0, this->point1, this->point2);
    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, -1));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = triangle.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.minWithPositiveDistance().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }
  
  TEST_F(TriangleTest, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    Triangle triangle(this->point0, this->point1, this->point2);
    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    
    State state;
    ASSERT_TRUE(triangle.intersects(ray, state));
  }
  
  TEST_F(TriangleTest, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Triangle triangle(this->point0, this->point1, this->point2);
    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, -1));
    
    State state;
    ASSERT_FALSE(triangle.intersects(ray, state));
  }
  
  TEST_F(TriangleTest, ShouldHaveSameNormalEverywhere) {
    Triangle triangle(this->point0, this->point1, this->point2);
    State state;
    HitPointInterval hitPoints1;
    Rayd ray1(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    triangle.intersect(ray1, hitPoints1, state);
    Vector3d normal1 = hitPoints1.min().normal();

    HitPointInterval hitPoints2;
    Rayd ray2(Vector3d(-1, -1, -1), Vector3d(0, 0, 1));
    triangle.intersect(ray2, hitPoints2, state);
    Vector3d normal2 = hitPoints2.min().normal();
    
    ASSERT_EQ(normal1, normal2);
  }

  TEST_F(TriangleTest, ShouldReturnBoundingBox) {
    Triangle triangle(this->point0, this->point1, this->point2);
    ASSERT_EQ(BoundingBoxd(Vector3d(-1, -1, 0), Vector3d(1, 1, 0)), triangle.boundingBox());
  }
}
