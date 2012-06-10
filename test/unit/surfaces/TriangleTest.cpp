#include "gtest.h"
#include "surfaces/Triangle.h"
#include "math/Ray.h"
#include "math/HitPointInterval.h"

namespace TriangleTest {
  using namespace ::testing;
  
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
    Ray ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    Surface* surface = triangle.intersect(ray, hitPoints);
    ASSERT_EQ(surface, &triangle);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
  }
  
  TEST_F(TriangleTest, ShouldNotIntersectWithMissingRay) {
    Triangle triangle(this->point0, this->point1, this->point2);
    Ray ray(Vector3d(0, 4, -1), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    Surface* surface = triangle.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, surface);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST_F(TriangleTest, ShouldNotIntersectIfPointIsBehindRayOrigin) {
    Triangle triangle(this->point0, this->point1, this->point2);
    Ray ray(Vector3d(0, 0, -1), Vector3d(0, 0, -1));
    
    HitPointInterval hitPoints;
    Surface* surface = triangle.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, surface);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TEST_F(TriangleTest, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    Triangle triangle(this->point0, this->point1, this->point2);
    Ray ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    
    ASSERT_TRUE(triangle.intersects(ray));
  }
  
  TEST_F(TriangleTest, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Triangle triangle(this->point0, this->point1, this->point2);
    Ray ray(Vector3d(0, 0, -1), Vector3d(0, 0, -1));
    
    ASSERT_FALSE(triangle.intersects(ray));
  }
  
  TEST_F(TriangleTest, ShouldHaveSameNormalEverywhere) {
    Triangle triangle(this->point0, this->point1, this->point2);
    HitPointInterval hitPoints1;
    Ray ray1(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    triangle.intersect(ray1, hitPoints1);
    Vector3d normal1 = hitPoints1.min().normal();

    HitPointInterval hitPoints2;
    Ray ray2(Vector3d(-1, -1, -1), Vector3d(0, 0, 1));
    triangle.intersect(ray2, hitPoints2);
    Vector3d normal2 = hitPoints2.min().normal();
    
    ASSERT_EQ(normal1, normal2);
  }
}
