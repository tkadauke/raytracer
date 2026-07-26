#include <gtest/gtest.h>
#include "render/brdf/PerfectSpecular.h"

#include "core/math/HitPoint.h"
#include "test/helpers/MaterialTestHelper.h"

namespace PerfectSpecularTest {
  using namespace render;
  using test::helpers::hitPointWithNormal;

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

    ASSERT_EQ(Colord::black(), specular.reflectance(HitPoint::undefined(), Vector3d::null));
  }

  TEST(PerfectSpecular, ShouldBeBlack) {
    PerfectSpecular specular;

    ASSERT_EQ(Colord::black(), specular(HitPoint::undefined(), Vector3d::null, Vector3d::null));
  }

  TEST(PerfectSpecular, ShouldExposeDeltaReflectionFlagsAndPdfContract) {
    PerfectSpecular specular;
    specular.setReflectionColor(Colord(0.25, 0.5, 1.0));
    specular.setReflectionCoefficient(0.7);

    HitPoint point = hitPointWithNormal(Vector3d::up());
    Vector3d sampledDirection;
    double sampledPdf = 0.0;
    Colord value = specular.sample(point, Vector3d::up(), sampledDirection, sampledPdf);

    ASSERT_TRUE(specular.isSpecular());
    ASSERT_TRUE(specular.isReflection());
    ASSERT_TRUE(specular.isDelta());
    ASSERT_FALSE(specular.isTransmission());
    ASSERT_NEAR(1.0, sampledPdf, 1e-12);
    ASSERT_TRUE(sampledDirection.approxEqual(Vector3d::up(), 1e-9));
    ASSERT_NEAR(0.0, specular.pdf(point, Vector3d::up(), sampledDirection), 1e-12);
    ASSERT_NEAR(0.25 * 0.7, value.r(), 1e-12);
    ASSERT_NEAR(0.5 * 0.7, value.g(), 1e-12);
    ASSERT_NEAR(1.0 * 0.7, value.b(), 1e-12);
  }
}
