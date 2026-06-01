#include <gtest/gtest.h>
#include "render/materials/ReflectiveMaterial.h"
#include "render/textures/ConstantColorTexture.h"

#include "core/math/HitPoint.h"
#include "render/State.h"
#include "render/primitives/Scene.h"

#include "test/helpers/ColorTestHelper.h"
#include "test/helpers/RecordingRayCaster.h"

namespace ReflectiveMaterialTest {
  using namespace render;
  using test::helpers::RecordingRayCaster;

  TEST(ReflectiveMaterial, ShouldInitialize) {
    ReflectiveMaterial material;
  }

  TEST(ReflectiveMaterial, ShouldInitializeWithDiffuseTexture) {
    auto texture = std::make_shared<ConstantColorTexture>(Colord(0, 1, 0));
    ReflectiveMaterial material(texture);
    ASSERT_EQ(texture, material.diffuseTexture());
    ASSERT_EQ(Colord::white(), material.specularColor());
  }

  TEST(ReflectiveMaterial, ShouldSetHighlightColor) {
    ReflectiveMaterial material;
    material.setSpecularColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.specularColor());
  }

  TEST(ReflectiveMaterial, ShouldSetReflectionColor) {
    ReflectiveMaterial material;
    material.setReflectionColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.reflectionColor());
  }

  TEST(ReflectiveMaterial, ShouldSetReflectionCoefficient) {
    ReflectiveMaterial material;
    material.setReflectionCoefficient(0.4);
    ASSERT_EQ(0.4, material.reflectionCoefficient());
  }

  TEST(ReflectiveMaterial, SupportsBsdfSamplingForPathTracing) {
    ReflectiveMaterial material;
    EXPECT_TRUE(material.supportsBsdfSampling());
  }

  TEST(ReflectiveMaterial, SamplesMirrorReflectionAsDeltaBsdf) {
    ReflectiveMaterial material;
    material.setAmbientCoefficient(0.0);
    material.setDiffuseCoefficient(0.0);
    material.setSpecularCoefficient(0.0);
    material.setReflectionColor(Colord(0, 1, 0));
    material.setReflectionCoefficient(0.25);
    HitPoint hitPoint{nullptr, 1.0, Vector4d(0, 0, 0, 1), Vector3d(0, 1, 0)};

    const MaterialBsdfSample sampled =
      material.sampleBsdf(hitPoint, Vector3d(0, 1, 0), Vector2d(0.75, 0.5));

    EXPECT_TRUE(sampled.isDelta);
    EXPECT_DOUBLE_EQ(1.0, sampled.pdf);
    ASSERT_COLOR_NEAR(Colord(0, 0.25, 0), sampled.value, 1e-12);
    EXPECT_NEAR(0.0, sampled.direction.x(), 1e-12);
    EXPECT_NEAR(1.0, sampled.direction.y(), 1e-12);
    EXPECT_NEAR(0.0, sampled.direction.z(), 1e-12);
  }

  TEST(ReflectiveMaterial, ShouldDescribeRasterRecursiveFallback) {
    ReflectiveMaterial material;
    ASSERT_EQ(Material::RasterRecursiveFallback::ReflectiveLocalPhong,
              material.rasterRecursiveFallback());
    ASSERT_EQ(1.0, material.rasterPreviewAlpha());
    ASSERT_STREQ("Rasterizer fallback: ReflectiveMaterial previews only its local Phong base; "
                 "mirror recursion remains raytracer-only.",
                 material.rasterRecursiveFallbackWarning());
  }

  // ---- shading-behaviour tests ---------------------------------------------
  //
  // ReflectiveMaterial::shade is PhongMaterial::shade plus a recursive call:
  //
  //   reflected = perfect mirror reflection of -ray.direction
  //   color += reflectionColor * reflectionCoefficient *
  //            raytracer->rayColor(reflected) * (N·in)
  //
  // (The /normalDotIn inside PerfectSpecular::sample cancels the *normalDotIn
  // in shade, so the reflection contribution simplifies to
  // reflectionColor * coeff * tracedColor.)
  //
  // The fixture below puts a known background colour on an otherwise empty
  // recursive `RayCaster` test double — the cleanest way to assert "we
  // reflected colour X" without depending on Raytracer internals.

  namespace {
    struct ShadeFixture {
      std::shared_ptr<ConstantColorTexture> texture =
        std::make_shared<ConstantColorTexture>(Colord::white());
      std::shared_ptr<render::Scene> scene = std::make_shared<Scene>(Colord::black());
      RecordingRayCaster raycaster;

      HitPoint hitPoint{nullptr, 1.0, Vector4d(0, 0, 0, 1), Vector3d(0, 1, 0)};
      Rayd ray{Vector3d(0, 5, 0), Vector3d(0, -1, 0)};
      State state;

      ReflectiveMaterial material{texture};

      ShadeFixture() {
        // Suppress the inherited Phong contribution so reflections show up
        // on their own — every test here is about the reflection branch.
        scene->setAmbient(Colord::black());
        material.setAmbientCoefficient(0.0);
        material.setDiffuseCoefficient(0.0);
        material.setSpecularCoefficient(0.0);
      }
    };
  }

  TEST(ReflectiveMaterial, ShouldReflectSceneBackground) {
    ShadeFixture f;
    f.raycaster.pushColor(Colord(1, 0, 0));
    f.material.setReflectionColor(Colord::white());
    f.material.setReflectionCoefficient(1.0);

    auto colour = f.material.shade(&f.raycaster, *f.scene, f.ray, f.hitPoint, f.state);

    // reflectionColor * coeff * background = white * 1 * red = red.
    ASSERT_COLOR_NEAR(Colord(1, 0, 0), colour, 0.001);
    ASSERT_EQ(1u, f.raycaster.rays.size());
    EXPECT_NEAR(0.0, f.raycaster.rays.front().direction().x(), 0.001);
    EXPECT_NEAR(1.0, f.raycaster.rays.front().direction().y(), 0.001);
    EXPECT_NEAR(0.0, f.raycaster.rays.front().direction().z(), 0.001);
  }

  TEST(ReflectiveMaterial, ShouldScaleReflectionByCoefficient) {
    ShadeFixture f;
    f.raycaster.pushColor(Colord(1, 1, 1));
    f.material.setReflectionColor(Colord::white());
    f.material.setReflectionCoefficient(0.25);

    auto colour = f.material.shade(&f.raycaster, *f.scene, f.ray, f.hitPoint, f.state);

    // 1.0 * 0.25 * (1,1,1) = (0.25, 0.25, 0.25).
    ASSERT_COLOR_NEAR(Colord(0.25, 0.25, 0.25), colour, 0.001);
  }

  TEST(ReflectiveMaterial, ShouldFilterReflectionByReflectionColor) {
    ShadeFixture f;
    f.raycaster.pushColor(Colord(1, 1, 1));
    // Tinted mirror: reflects only the green channel.
    f.material.setReflectionColor(Colord(0, 1, 0));
    f.material.setReflectionCoefficient(1.0);

    auto colour = f.material.shade(&f.raycaster, *f.scene, f.ray, f.hitPoint, f.state);

    // (0, 1, 0) * 1 * (1, 1, 1) = (0, 1, 0).
    ASSERT_COLOR_NEAR(Colord(0, 1, 0), colour, 0.001);
  }
}
