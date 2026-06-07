#include <gtest/gtest.h>

#include "core/math/Constants.h"
#include "core/math/HitPointInterval.h"
#include "render/State.h"
#include "render/materials/EmissiveMaterial.h"
#include "render/lights/RectangularAreaLight.h"
#include "render/primitives/Primitive.h"

#include "test/helpers/ColorTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

namespace RectangularAreaLightTest {
  using namespace render;

  TEST(RectangularAreaLight, SamplesPointOnEmittingSide) {
    RectangularAreaLight light(Vector3d(0, 2, 0), Vector3d(2, 0, 0), Vector3d(0, 0, 2),
                               Colord(0.25, 0.5, 0.75));

    const LightSample sample = light.sample(Vector3d::null, Vector2d(0.5, 0.5));

    ASSERT_VECTOR_NEAR(Vector3d(0, 1, 0), sample.direction, 1e-12);
    EXPECT_EQ(Colord(0.25, 0.5, 0.75), sample.radiance);
    EXPECT_DOUBLE_EQ(2.0, sample.distance);
    EXPECT_DOUBLE_EQ(1.0, sample.pdf);
    EXPECT_FALSE(sample.delta);
  }

  TEST(RectangularAreaLight, ConvertsAreaPdfToSolidAnglePdf) {
    RectangularAreaLight light(Vector3d(0, 2, 0), Vector3d(2, 0, 0), Vector3d(0, 0, 2),
                               Colord::white());

    const LightSample sample = light.sample(Vector3d::null, Vector2d(1.0, 0.5));

    ASSERT_VECTOR_NEAR(Vector3d(1, 2, 0).normalized(), sample.direction, 1e-12);
    EXPECT_DOUBLE_EQ(std::sqrt(5.0), sample.distance);
    EXPECT_NEAR(5.0 * std::sqrt(5.0) / 8.0, sample.pdf, 1e-12);
    EXPECT_NEAR(sample.pdf, light.pdf(Vector3d::null, sample.direction), 1e-12);
  }

  TEST(RectangularAreaLight, RejectsBackSideSamples) {
    RectangularAreaLight light(Vector3d(0, 2, 0), Vector3d(2, 0, 0), Vector3d(0, 0, 2),
                               Colord::white());

    const LightSample sample = light.sample(Vector3d(0, 4, 0), Vector2d(0.5, 0.5));

    EXPECT_EQ(Colord::black(), sample.radiance);
    EXPECT_DOUBLE_EQ(0.0, sample.pdf);
    EXPECT_DOUBLE_EQ(0.0, light.pdf(Vector3d(0, 4, 0), Vector3d(0, -1, 0)));
  }

  TEST(RectangularAreaLight, RejectsDirectionsOutsideRectangle) {
    RectangularAreaLight light(Vector3d(0, 2, 0), Vector3d(2, 0, 0), Vector3d(0, 0, 2),
                               Colord::white());

    EXPECT_DOUBLE_EQ(0.0, light.pdf(Vector3d::null, Vector3d(2, 2, 0).normalized()));
  }

  TEST(RectangularAreaLight, ExposesAreaEmissionAndPower) {
    RectangularAreaLight light(Vector3d(0, 2, 0), Vector3d(3, 0, 0), Vector3d(0, 0, 2),
                               Colord(0.25, 0.5, 0.75));

    EXPECT_FALSE(light.isDelta());
    EXPECT_DOUBLE_EQ(6.0, light.area());
    EXPECT_EQ(Colord(0.25, 0.5, 0.75), light.emission());
    ASSERT_TRUE(light.power().has_value());
    EXPECT_EQ(Colord(0.25, 0.5, 0.75) * 6.0 * PI, *light.power());
  }

  TEST(RectangularAreaLight, ExposesVisibleEmitterPrimitive) {
    RectangularAreaLight light(Vector3d(0, 2, 0), Vector3d(2, 0, 0), Vector3d(0, 0, 2),
                               Colord(0.25, 0.5, 0.75));

    const auto emitter = light.emitterPrimitive();
    ASSERT_NE(nullptr, emitter);
    ASSERT_NE(nullptr, emitter->material());

    State state;
    HitPointInterval hitPoints;
    const Rayd ray(Vector3d::null, Vector3d(0, 1, 0));
    ASSERT_EQ(emitter.get(), emitter->intersect(ray, hitPoints, state));

    const HitPoint hitPoint = hitPoints.minWithPositiveDistance();
    ASSERT_FALSE(hitPoint.isUndefined());
    ASSERT_COLOR_NEAR(Colord(0.25, 0.5, 0.75), emitter->material()->emittedRadiance(ray, hitPoint),
                      1e-12);
  }
}
