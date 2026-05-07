#include <gtest/gtest.h>
#include "render/materials/ReflectiveMaterial.h"
#include "render/textures/ConstantColorTexture.h"

#include "core/math/HitPoint.h"
#include "engine/raytracer/Raytracer.h"
#include "render/State.h"
#include "render/primitives/Scene.h"

#include "test/helpers/ColorTestHelper.h"

namespace ReflectiveMaterialTest {
  using namespace render;
  using namespace engine::raytracer;
using namespace render;
  using namespace engine::raytracer;
using namespace render;
  using namespace engine::raytracer;

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
  // scene so the recursive ray hits nothing and rayColor returns the
  // background — the cleanest way to assert "we reflected colour X".

  namespace {
    struct ShadeFixture {
      std::shared_ptr<ConstantColorTexture> texture =
        std::make_shared<ConstantColorTexture>(Colord::white());
      std::shared_ptr<render::Scene> scene = std::make_shared<Scene>(Colord::black());
      std::shared_ptr<Raytracer> raytracer = std::make_shared<Raytracer>(scene);

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
    f.scene->setBackground(Colord(1, 0, 0));
    f.material.setReflectionColor(Colord::white());
    f.material.setReflectionCoefficient(1.0);

    auto colour = f.material.shade(f.raytracer.get(), *f.scene, f.ray, f.hitPoint, f.state);

    // Reflected ray hits no primitive → rayColor returns the background.
    // reflectionColor * coeff * background = white * 1 * red = red.
    ASSERT_COLOR_NEAR(Colord(1, 0, 0), colour, 0.001);
  }

  TEST(ReflectiveMaterial, ShouldScaleReflectionByCoefficient) {
    ShadeFixture f;
    f.scene->setBackground(Colord(1, 1, 1));
    f.material.setReflectionColor(Colord::white());
    f.material.setReflectionCoefficient(0.25);

    auto colour = f.material.shade(f.raytracer.get(), *f.scene, f.ray, f.hitPoint, f.state);

    // 1.0 * 0.25 * (1,1,1) = (0.25, 0.25, 0.25).
    ASSERT_COLOR_NEAR(Colord(0.25, 0.25, 0.25), colour, 0.001);
  }

  TEST(ReflectiveMaterial, ShouldFilterReflectionByReflectionColor) {
    ShadeFixture f;
    f.scene->setBackground(Colord(1, 1, 1));
    // Tinted mirror: reflects only the green channel.
    f.material.setReflectionColor(Colord(0, 1, 0));
    f.material.setReflectionCoefficient(1.0);

    auto colour = f.material.shade(f.raytracer.get(), *f.scene, f.ray, f.hitPoint, f.state);

    // (0, 1, 0) * 1 * (1, 1, 1) = (0, 1, 0).
    ASSERT_COLOR_NEAR(Colord(0, 1, 0), colour, 0.001);
  }
}
