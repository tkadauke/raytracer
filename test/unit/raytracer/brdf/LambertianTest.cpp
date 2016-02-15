#include "gtest.h"
#include "raytracer/brdf/Lambertian.h"

#include "core/math/HitPoint.h"

namespace LambertianTest {
  using namespace raytracer;

  TEST(Lambertian, ShouldInitialize) {
    Lambertian lambertian;
    ASSERT_EQ(1, lambertian.reflectionCoefficient());
  }
  
  TEST(Lambertian, ShouldSetDiffuseColor) {
    Lambertian lambertian;
    lambertian.setDiffuseColor(Colord(1, 0, 0));
    ASSERT_EQ(Colord(1, 0, 0), lambertian.diffuseColor());
  }
  
  TEST(Lambertian, ShouldSetReflectionCoefficient) {
    Lambertian lambertian;
    lambertian.setReflectionCoefficient(0.2);
    ASSERT_EQ(0.2, lambertian.reflectionCoefficient());

    lambertian.setReflectionCoefficient(-4);
    ASSERT_EQ(0, lambertian.reflectionCoefficient());

    lambertian.setReflectionCoefficient(12);
    ASSERT_EQ(1, lambertian.reflectionCoefficient());
  }
  
  TEST(Lambertian, ShouldBeIndependentOfRayDirection) {
    Lambertian lambertian;
    lambertian.setDiffuseColor(Colord(1, 0, 0));
    
    ASSERT_EQ(Colord(1, 0, 0), lambertian.reflectance(HitPoint::undefined(), Vector3d::null()));
  }
}
