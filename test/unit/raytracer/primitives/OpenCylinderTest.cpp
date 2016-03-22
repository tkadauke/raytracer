#include "gtest/gtest.h"
#include "raytracer/State.h"
#include "raytracer/primitives/OpenCylinder.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

namespace OpenCylinderTest {
  using namespace raytracer;

  TEST(OpenCylinder, ShouldInitializeWithValues) {
    OpenCylinder cylinder(1, 2);
  }
  
  TEST(OpenCylinder, ShouldIntersectWithRayInXDirection) {
    OpenCylinder cylinder(1, 2);
    Rayd ray(Vector3d(-2, 0, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = cylinder.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &cylinder);
    ASSERT_EQ(Vector3d(-1, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(-1, 0, 0), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }
  
  TEST(OpenCylinder, ShouldNotIntersectWithRayInYDirection) {
    OpenCylinder cylinder(1, 2);
    Rayd ray(Vector3d(0, -2, 0), Vector3d(0, 1, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = cylinder.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }
  
  TEST(OpenCylinder, ShouldIntersectWithRayInZDirection) {
    OpenCylinder cylinder(1, 2);
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = cylinder.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &cylinder);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }
  
  TEST(OpenCylinder, ShouldIntersectIfRayIsTangentToPrimitive) {
    OpenCylinder cylinder(1, 2);
    Rayd ray(Vector3d(0, 1, -2), Vector3d(0, 0, 1));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = cylinder.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &cylinder);
    ASSERT_EQ(Vector3d(0, 1, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }
  
  TEST(OpenCylinder, ShouldNotIntersectWithMissingRay) {
    OpenCylinder cylinder(1, 2);
    Rayd ray(Vector3d(0, 0, -3), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = cylinder.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }
  
  TEST(OpenCylinder, ShouldNotIntersectIfOpenCylinderIsBehindRayOrigin) {
    OpenCylinder cylinder(1, 2);
    Rayd ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = cylinder.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.minWithPositiveDistance().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }
  
  TEST(OpenCylinder, ShouldReportBothHitpointsWhenRayOriginIsInsideOpenCylinder) {
    OpenCylinder cylinder(1, 2);
    Rayd ray(Vector3d(0, 0, 0), Vector3d(0, 0, 1));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = cylinder.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &cylinder);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(-1, hitPoints.min().distance());

    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().point());
    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().normal());
    ASSERT_EQ(1, hitPoints.max().distance());

    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }
  
  TEST(OpenCylinder, ShouldReturnBoundingBox) {
    OpenCylinder cylinder(1, 2);
    BoundingBoxd bbox = cylinder.boundingBox();
    ASSERT_EQ(Vector3d(-1, -1, -1), bbox.min());
    ASSERT_EQ(Vector3d(1, 1, 1), bbox.max());
  }
}
