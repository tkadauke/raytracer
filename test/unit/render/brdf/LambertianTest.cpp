#include <gtest/gtest.h>
#include "render/brdf/Lambertian.h"

#include "core/math/Constants.h"
#include "core/math/HitPoint.h"

namespace LambertianTest {
  using namespace render;
  using namespace render;
  using namespace render;

  TEST(Lambertian, ShouldInitialize) {
    Lambertian lambertian;
    ASSERT_EQ(1, lambertian.reflectionCoefficient());
  }

  TEST(Lambertian, ShouldInitializeWithValues) {
    Lambertian lambertian(Colord(0, 1, 0), 0.2);
    ASSERT_EQ(Colord(0, 1, 0), lambertian.diffuseColor());
    ASSERT_EQ(0.2, lambertian.reflectionCoefficient());
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

    ASSERT_EQ(Colord(1, 0, 0), lambertian.reflectance(HitPoint::undefined(), Vector3d::null));
  }

  TEST(Lambertian, ShouldExposeDiffuseReflectionFlags) {
    Lambertian lambertian;

    ASSERT_TRUE(lambertian.isDiffuse());
    ASSERT_TRUE(lambertian.isReflection());
    ASSERT_FALSE(lambertian.isDelta());
    ASSERT_FALSE(lambertian.isTransmission());
  }

  TEST(Lambertian, ShouldSampleCosineWeightedDirectionWithMatchingPdf) {
    Lambertian lambertian(Colord(0.5, 0.25, 1.0), 0.8);
    HitPoint point(nullptr, 1, Vector4d::null, Vector3d::up());

    Vector3d sampledDirection;
    double sampledPdf = 0.0;
    const Colord value =
      lambertian.sample(point, Vector3d::up(), sampledDirection, sampledPdf, Vector2d(0.0, 0.0));

    ASSERT_TRUE(sampledDirection.approxEqual(Vector3d::up(), 1e-9));
    ASSERT_NEAR(invPI, sampledPdf, 1e-12);
    ASSERT_NEAR(sampledPdf, lambertian.pdf(point, Vector3d::up(), sampledDirection), 1e-12);
    ASSERT_NEAR(0.5 * 0.8 * invPI, value.r(), 1e-12);
    ASSERT_NEAR(0.25 * 0.8 * invPI, value.g(), 1e-12);
    ASSERT_NEAR(1.0 * 0.8 * invPI, value.b(), 1e-12);
  }
}
