#include "gtest.h"
#include "raytracer/brdf/PerfectSpecular.h"

#include "core/math/HitPoint.h"

namespace PerfectSpecularTest {
  using namespace raytracer;

  TEST(PerfectSpecular, ShouldInitialize) {
    PerfectSpecular specular;
    ASSERT_EQ(1, specular.reflectionCoefficient());
  }
  
  TEST(PerfectSpecular, ShouldSetReflectionColor) {
    PerfectSpecular specular;
    specular.setReflectionColor(Colord(1, 0, 0));
    ASSERT_EQ(Colord(1, 0, 0), specular.reflectionColor());
  }

  TEST(PerfectSpecular, ShouldSetReflectionCoefficient) {
    PerfectSpecular specular;
    specular.setReflectionCoefficient(0.2);
    ASSERT_EQ(0.2, specular.reflectionCoefficient());

    specular.setReflectionCoefficient(-4);
    ASSERT_EQ(0, specular.reflectionCoefficient());

    specular.setReflectionCoefficient(12);
    ASSERT_EQ(1, specular.reflectionCoefficient());
  }

  TEST(PerfectSpecular, ShouldHaveBlackReflectance) {
    PerfectSpecular specular;
    
    ASSERT_EQ(Colord::black(), specular.reflectance(HitPoint::undefined(), Vector3d::null()));
  }

  TEST(PerfectSpecular, ShouldBeBlack) {
    PerfectSpecular specular;
    
    ASSERT_EQ(Colord::black(), specular(HitPoint::undefined(), Vector3d::null(), Vector3d::null()));
  }
}
