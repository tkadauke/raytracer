#include <gtest/gtest.h>

#include "core/math/HitPoint.h"
#include "render/materials/EmissiveMaterial.h"

#include "test/helpers/ColorTestHelper.h"
#include "test/helpers/MaterialTestHelper.h"

namespace EmissiveMaterialTest {
  using namespace render;
  using test::helpers::hitPointWithNormal;

  TEST(EmissiveMaterial, EmitsTowardFrontSide) {
    EmissiveMaterial material(Colord(2.0, 3.0, 4.0));
    const HitPoint hitPoint = hitPointWithNormal(Vector3d(0, -1, 0));
    const Rayd ray(Vector3d(0, -2, 0), Vector3d(0, 1, 0));

    ASSERT_COLOR_NEAR(Colord(2.0, 3.0, 4.0), material.emittedRadiance(ray, hitPoint), 1e-12);
  }

  TEST(EmissiveMaterial, DoesNotEmitTowardBackSide) {
    EmissiveMaterial material(Colord(2.0, 3.0, 4.0));
    const HitPoint hitPoint = hitPointWithNormal(Vector3d(0, -1, 0));
    const Rayd ray(Vector3d(0, 2, 0), Vector3d(0, -1, 0));

    ASSERT_COLOR_NEAR(Colord::black(), material.emittedRadiance(ray, hitPoint), 1e-12);
  }

  TEST(EmissiveMaterial, ExposesNoBsdfContinuation) {
    EmissiveMaterial material(Colord(2.0, 3.0, 4.0));
    const HitPoint hitPoint = hitPointWithNormal(Vector3d(0, -1, 0));

    EXPECT_TRUE(material.supportsPathTracing());
    EXPECT_FALSE(material.supportsBsdfSampling());
    EXPECT_EQ(Colord::black(), material.evalBsdf(hitPoint, Vector3d(0, 1, 0), Vector3d(1, 0, 0)));
    EXPECT_DOUBLE_EQ(0.0, material.bsdfPdf(hitPoint, Vector3d(0, 1, 0), Vector3d(1, 0, 0)));
    EXPECT_DOUBLE_EQ(0.0, material.sampleBsdf(hitPoint, Vector3d(0, 1, 0), Vector2d(0.5, 0.5)).pdf);
  }
}
