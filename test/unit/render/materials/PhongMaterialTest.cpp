#include <gtest/gtest.h>
#include "render/materials/PhongMaterial.h"
#include "render/textures/ConstantColorTexture.h"

#include "core/math/HitPoint.h"
#include "engine/raytracer/Raytracer.h"
#include "render/State.h"
#include "render/lights/DirectionalLight.h"
#include "render/lights/PointLight.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"

#include "test/helpers/ColorTestHelper.h"

namespace PhongMaterialTest {
  using namespace render;
  using namespace engine::raytracer;
  using namespace render;
  using namespace engine::raytracer;
  using namespace render;
  using namespace engine::raytracer;

  TEST(PhongMaterial, ShouldInitialize) {
    PhongMaterial material;
    ASSERT_EQ(Colord::white(), material.specularColor());
  }

  TEST(PhongMaterial, ShouldInitializeWithDiffuseTexture) {
    auto texture = std::make_shared<ConstantColorTexture>(Colord(0, 1, 0));
    PhongMaterial material(texture);
    ASSERT_EQ(texture, material.diffuseTexture());
    ASSERT_EQ(Colord::white(), material.specularColor());
  }

  TEST(PhongMaterial, ShouldSetHighlightColor) {
    PhongMaterial material;
    material.setSpecularColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.specularColor());
  }

  TEST(PhongMaterial, ShouldSetSpecularCoefficient) {
    PhongMaterial material;
    material.setSpecularCoefficient(0.4);
    ASSERT_EQ(0.4, material.specularCoefficient());
  }

  TEST(PhongMaterial, ShouldSetExponent) {
    PhongMaterial material;
    material.setExponent(64);
    ASSERT_EQ(64, material.exponent());
  }

  TEST(PhongMaterial, SupportsBsdfSamplingForPathTracing) {
    PhongMaterial material;
    EXPECT_TRUE(material.supportsWhittedContinuations());
    EXPECT_TRUE(material.supportsBsdfSampling());
  }

  TEST(PhongMaterial, EvaluatesDiffuseAndGlossyBsdfLobes) {
    auto texture = std::make_shared<ConstantColorTexture>(Colord::white());
    PhongMaterial material(texture);
    material.setDiffuseCoefficient(1.0);
    material.setSpecularCoefficient(1.0);
    material.setSpecularColor(Colord::white());
    material.setExponent(16);
    HitPoint hitPoint{nullptr, 1.0, Vector4d(0, 0, 0, 1), Vector3d(0, 1, 0)};

    const Colord value = material.evalBsdf(hitPoint, Vector3d(0, 1, 0), Vector3d(0, 1, 0));

    const double expected = 1.0 / M_PI + 1.0;
    ASSERT_COLOR_NEAR(Colord(expected, expected, expected), value, 1e-12);
  }

  TEST(PhongMaterial, SamplesDiffuseAndGlossyBsdfWithMixturePdf) {
    auto texture = std::make_shared<ConstantColorTexture>(Colord::white());
    PhongMaterial material(texture);
    material.setDiffuseCoefficient(1.0);
    material.setSpecularCoefficient(1.0);
    material.setSpecularColor(Colord::white());
    material.setExponent(16);
    HitPoint hitPoint{nullptr, 1.0, Vector4d(0, 0, 0, 1), Vector3d(0, 1, 0)};

    const MaterialBsdfSample sampled =
      material.sampleBsdf(hitPoint, Vector3d(0, 1, 0), Vector2d(0.75, 0.5));

    EXPECT_FALSE(sampled.isDelta);
    EXPECT_GT(sampled.pdf, 0.0);
    EXPECT_GT(sampled.value.max(), 0.0);
    EXPECT_DOUBLE_EQ(material.bsdfPdf(hitPoint, Vector3d(0, 1, 0), sampled.direction), sampled.pdf);
  }

  // ---- shading-behaviour tests ---------------------------------------------
  //
  // PhongMaterial::shade extends MatteMaterial::shade with a glossy-specular
  // term: per light, the specular contribution is
  //
  //   specularColor * specularCoeff * pow(R · V, exponent)   if R·V > 0
  //
  // where R is the ideal mirror reflection of the light direction around
  // the surface normal and V is the direction toward the viewer (-ray.dir).

  namespace {
    struct ShadeFixture {
      std::shared_ptr<ConstantColorTexture> texture =
        std::make_shared<ConstantColorTexture>(Colord::white());
      std::shared_ptr<render::Scene> scene = std::make_shared<Scene>();
      std::shared_ptr<Raytracer> raytracer = std::make_shared<Raytracer>(scene);

      // Upward-facing surface, viewer directly above looking down.
      HitPoint hitPoint{nullptr, 1.0, Vector4d(0, 0, 0, 1), Vector3d(0, 1, 0)};
      Rayd ray{Vector3d(0, 5, 0), Vector3d(0, -1, 0)};
      State state;

      PhongMaterial material{texture};
    };
  }

  TEST(PhongMaterial, ShouldProducePeakSpecularAtMirrorAngle) {
    ShadeFixture f;
    f.scene->setAmbient(Colord::black());
    // Isolate the specular term: ambient and diffuse contribute zero.
    f.material.setAmbientCoefficient(0.0);
    f.material.setDiffuseCoefficient(0.0);
    f.material.setSpecularColor(Colord(0.5, 0.5, 0.5));
    f.material.setSpecularCoefficient(1.0);
    // Light directly overhead. With viewer also directly overhead, R = N
    // and R·V = 1, so pow(1, exp) = 1 regardless of exponent — peak.
    f.scene->addLight(std::make_shared<DirectionalLight>(Vector3d(0, 1, 0), Colord(1, 1, 1)));

    auto colour = f.material.shade(f.raytracer.get(), *f.scene, f.ray, f.hitPoint, f.state);

    // Specular peak: specularColor * coeff * 1 * radiance * (N·L) =
    // 0.5 * 1 * 1 * 1 * 1 per channel.
    ASSERT_COLOR_NEAR(Colord(0.5, 0.5, 0.5), colour, 0.001);
  }

  TEST(PhongMaterial, ShouldFallOffAwayFromMirrorAngle) {
    ShadeFixture f;
    f.scene->setAmbient(Colord::black());
    f.material.setAmbientCoefficient(0.0);
    f.material.setDiffuseCoefficient(0.0);
    f.material.setSpecularColor(Colord::white());
    f.material.setSpecularCoefficient(1.0);
    f.material.setExponent(16);
    // Same overhead light, but now the viewer is at 45° from the surface
    // normal. R = (0, 1, 0); V = (sin 45°, cos 45°, 0); R·V = cos 45° ≈
    // 0.707; specular = 0.707^16 ≈ 0.0048 per channel — small but non-zero.
    constexpr double s = 0.7071067811865476; // sin/cos 45°
    f.ray = Rayd(Vector3d(5, 5, 0), Vector3d(-s, -s, 0));
    f.scene->addLight(std::make_shared<DirectionalLight>(Vector3d(0, 1, 0), Colord(1, 1, 1)));

    auto colour = f.material.shade(f.raytracer.get(), *f.scene, f.ray, f.hitPoint, f.state);

    constexpr double expected = 0.7071067811865476; // cos 45°
    double pow16 = 1.0;
    for (int i = 0; i < 16; ++i)
      pow16 *= expected;
    ASSERT_COLOR_NEAR(Colord(pow16, pow16, pow16), colour, 0.001);
  }

  TEST(PhongMaterial, ShouldAddAmbientDiffuseAndSpecular) {
    ShadeFixture f;
    f.scene->setAmbient(Colord(0.1, 0.1, 0.1));
    f.material.setAmbientCoefficient(1.0);
    f.material.setDiffuseCoefficient(1.0);
    f.material.setSpecularColor(Colord::white());
    f.material.setSpecularCoefficient(0.5);
    f.scene->addLight(std::make_shared<DirectionalLight>(Vector3d(0, 1, 0), Colord(1, 1, 1)));

    auto colour = f.material.shade(f.raytracer.get(), *f.scene, f.ray, f.hitPoint, f.state);

    // ambient = 1*1*0.1 = 0.1
    // diffuse = (1/π)*1*1 = 1/π ≈ 0.318
    // specular = 1*0.5*pow(1, 16)*1*1 = 0.5
    // total ≈ 0.918 per channel.
    constexpr double kInvPi = 1.0 / M_PI;
    double expected = 0.1 + kInvPi + 0.5;
    ASSERT_COLOR_NEAR(Colord(expected, expected, expected), colour, 0.001);
  }

  TEST(PhongMaterial, ShouldIgnorePointLightOccluderBehindTheLight) {
    ShadeFixture f;
    f.scene->setAmbient(Colord::black());
    f.material.setAmbientCoefficient(0.0);
    f.material.setDiffuseCoefficient(1.0);
    f.material.setSpecularCoefficient(0.0);
    f.scene->addLight(std::make_shared<PointLight>(Vector3d(0, 2, 0), Colord(0.5, 0.5, 0.5)));
    f.scene->add(std::make_shared<Sphere>(Vector3d(0, 4, 0), 0.5));

    auto colour = f.material.shade(f.raytracer.get(), *f.scene, f.ray, f.hitPoint, f.state);

    constexpr double kInvPi = 1.0 / M_PI;
    ASSERT_COLOR_NEAR(Colord(0.5 * kInvPi, 0.5 * kInvPi, 0.5 * kInvPi), colour, 0.001);
  }
}
