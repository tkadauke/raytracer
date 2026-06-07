#include <gtest/gtest.h>

#include "render/lights/DirectionalLight.h"
#include "render/lights/Light.h"
#include "render/lights/LightSampler.h"
#include "render/lights/PointLight.h"
#include "render/primitives/Scene.h"

namespace LightSamplerTest {
  using namespace render;

  namespace {
    class FixedPdfLight final : public Light {
    public:
      FixedPdfLight(double weight, double density)
          : m_weight(weight),
            m_density(density) {
      }

      Vector3d direction(const Vector3d&) const override {
        return Vector3d(0, 1, 0);
      }

      Colord radiance() const override {
        return Colord(m_weight, 0.0, 0.0);
      }

      double pdf(const Vector3d&, const Vector3d&) const override {
        return m_density;
      }

      bool isDelta() const override {
        return false;
      }

      const char* fingerprintType() const override {
        return "FixedPdfLight";
      }

    private:
      double m_weight;
      double m_density;
    };
  }

  TEST(LightSampler, ReturnsEmptySelectionForEmptyScene) {
    Scene scene;
    LightSampler sampler(scene.lights());

    EXPECT_TRUE(sampler.empty());
    EXPECT_FALSE(sampler.select(0.5));
  }

  TEST(LightSampler, SelectsLightsByPowerWeight) {
    Scene scene;
    scene.addLight(std::make_shared<PointLight>(Vector3d::null, Colord(1.0, 0.0, 0.0)));
    scene.addLight(std::make_shared<PointLight>(Vector3d::null, Colord(3.0, 0.0, 0.0)));

    LightSampler sampler(scene.lights());

    EXPECT_FALSE(sampler.empty());
    EXPECT_EQ(2u, sampler.size());

    const LightSampler::Selection first = sampler.select(0.20);
    ASSERT_TRUE(first);
    EXPECT_EQ(0u, first.lightIndex);
    EXPECT_DOUBLE_EQ(0.25, first.pdf);

    const LightSampler::Selection second = sampler.select(0.26);
    ASSERT_TRUE(second);
    EXPECT_EQ(1u, second.lightIndex);
    EXPECT_DOUBLE_EQ(0.75, second.pdf);
  }

  TEST(LightSampler, UsesEmissionForUnboundedLights) {
    Scene scene;
    scene.addLight(std::make_shared<DirectionalLight>(Vector3d(0, -1, 0), Colord(2.0, 0.0, 0.0)));
    scene.addLight(std::make_shared<DirectionalLight>(Vector3d(0, -1, 0), Colord(6.0, 0.0, 0.0)));

    LightSampler sampler(scene.lights());

    EXPECT_EQ(0u, sampler.select(0.24).lightIndex);
    EXPECT_EQ(1u, sampler.select(0.25).lightIndex);
    EXPECT_DOUBLE_EQ(0.25, sampler.selectionPdf(0));
    EXPECT_DOUBLE_EQ(0.75, sampler.selectionPdf(1));
  }

  TEST(LightSampler, FallsBackToUniformWhenPublishedWeightsAreZero) {
    Scene scene;
    scene.addLight(std::make_shared<PointLight>(Vector3d::null, Colord::black()));
    scene.addLight(std::make_shared<PointLight>(Vector3d::null, Colord::black()));

    LightSampler sampler(scene.lights());

    EXPECT_EQ(0u, sampler.select(0.49).lightIndex);
    EXPECT_EQ(1u, sampler.select(0.50).lightIndex);
    EXPECT_DOUBLE_EQ(0.5, sampler.selectionPdf(0));
    EXPECT_DOUBLE_EQ(0.5, sampler.selectionPdf(1));
  }

  TEST(LightSampler, CombinesLightPdfWithSelectionPdf) {
    Scene scene;
    scene.addLight(std::make_shared<FixedPdfLight>(1.0, 0.2));
    scene.addLight(std::make_shared<FixedPdfLight>(3.0, 0.4));

    LightSampler sampler(scene.lights());

    EXPECT_DOUBLE_EQ(0.35, sampler.pdf(Vector3d::null, Vector3d(0, 1, 0)));
  }
}
