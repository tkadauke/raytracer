#include <gtest/gtest.h>
#include "render/materials/MatteMaterial.h"
#include "render/textures/ConstantColorTexture.h"

#include "core/math/HitPoint.h"
#include "engine/raytracer/Raytracer.h"
#include "render/State.h"
#include "render/lights/DirectionalLight.h"
#include "render/primitives/Scene.h"

#include "test/helpers/ColorTestHelper.h"

namespace MatteMaterialTest {
  using namespace render;
  using namespace engine::raytracer;
using namespace render;
  using namespace engine::raytracer;
using namespace render;
  using namespace engine::raytracer;

  TEST(MatteMaterial, ShouldInitialize) {
    MatteMaterial material;
    ASSERT_EQ(nullptr, material.diffuseTexture());
  }

  TEST(MatteMaterial, ShouldInitializeWithDiffuseTexture) {
    auto texture = std::make_shared<ConstantColorTexture>(Colord(0, 1, 0));
    MatteMaterial material(texture);
    ASSERT_EQ(texture, material.diffuseTexture());
  }

  TEST(MatteMaterial, ShouldSetDiffuseTexture) {
    MatteMaterial material;

    auto texture = std::make_shared<ConstantColorTexture>(Colord(0, 1, 0));
    material.setDiffuseTexture(texture);
    ASSERT_EQ(texture, material.diffuseTexture());
  }

  TEST(MatteMaterial, ShouldSetAmbientCoefficient) {
    MatteMaterial material;
    material.setAmbientCoefficient(0.6);
    ASSERT_EQ(0.6, material.ambientCoefficient());
  }

  TEST(MatteMaterial, ShouldSetDiffuseCoefficient) {
    MatteMaterial material;
    material.setDiffuseCoefficient(0.6);
    ASSERT_EQ(0.6, material.diffuseCoefficient());
  }

  // ---- shading-behaviour tests ---------------------------------------------
  //
  // MatteMaterial::shade computes
  //
  //   ambient_term  = texColor * ambientCoeff * scene.ambient()
  //   diffuse_term  = sum over lights of:
  //                     0                                    if shadowed,
  //                     0                                    if N·L <= 0,
  //                     texColor * diffuseCoeff * radiance * (N·L)   otherwise
  //
  // and returns ambient_term + diffuse_term. The fixture below builds a
  // Raytracer with a configurable scene + light list so each test can
  // exercise one branch of that sum without having to reason about the
  // others.

  namespace {
    struct ShadeFixture {
      // White texture so the texture-modulation factor is identity.
      std::shared_ptr<ConstantColorTexture> texture =
        std::make_shared<ConstantColorTexture>(Colord::white());
      std::shared_ptr<render::Scene> scene = std::make_shared<Scene>();
      std::shared_ptr<Raytracer> raytracer = std::make_shared<Raytracer>(scene);

      // A horizontal hit point with the surface normal pointing straight up,
      // so a light coming from straight above contributes its full radiance.
      HitPoint hitPoint{nullptr, 1.0, Vector4d(0, 0, 0, 1), Vector3d(0, 1, 0)};
      Rayd ray{Vector3d(0, 5, 0), Vector3d(0, -1, 0)};
      State state;

      MatteMaterial material{texture};
    };
  }

  TEST(MatteMaterial, ShouldReturnAmbientOnlyWhenSceneHasNoLights) {
    ShadeFixture f;
    f.scene->setAmbient(Colord(0.25, 0.25, 0.25));
    f.material.setAmbientCoefficient(1.0);

    // No lights → diffuse term is zero, only the ambient term contributes.
    ASSERT_COLOR_NEAR(Colord(0.25, 0.25, 0.25),
                       f.material.shade(f.raytracer.get(), *f.scene, f.ray, f.hitPoint, f.state),
                       0.001);
  }

  TEST(MatteMaterial, ShouldAddDiffuseFromHeadOnDirectionalLight) {
    ShadeFixture f;
    f.scene->setAmbient(Colord::black());
    f.material.setAmbientCoefficient(0.0);
    f.material.setDiffuseCoefficient(1.0);
    // DirectionalLight's `direction` argument is the vector *toward* the
    // light source (matching PointLight's `position - point` convention),
    // not the direction the light is travelling. (0, 1, 0) means "the sun
    // is straight overhead": N·L = 1 with the upward-facing surface.
    f.scene->addLight(std::make_shared<DirectionalLight>(
      Vector3d(0, 1, 0), Colord(0.5, 0.5, 0.5)));

    auto colour = f.material.shade(f.raytracer.get(), *f.scene, f.ray, f.hitPoint, f.state);

    // Lambertian BRDF returns diffuseColor * coeff * (1/π); white texture
    // and coeff 1.0 give 1/π, scaled by the light's radiance and N·L = 1.
    constexpr double kInvPi = 1.0 / M_PI;
    ASSERT_COLOR_NEAR(Colord(0.5 * kInvPi, 0.5 * kInvPi, 0.5 * kInvPi),
                       colour, 0.001);
  }

  TEST(MatteMaterial, ShouldIgnoreLightHittingTheBackOfTheSurface) {
    ShadeFixture f;
    f.scene->setAmbient(Colord::black());
    f.material.setAmbientCoefficient(0.0);
    f.material.setDiffuseCoefficient(1.0);
    // Direction toward the light points *down*, i.e. the light source is
    // below an upward-facing surface. N·L = -1, so the diffuse branch is
    // skipped and the result is black.
    f.scene->addLight(std::make_shared<DirectionalLight>(
      Vector3d(0, -1, 0), Colord(1, 1, 1)));

    ASSERT_COLOR_NEAR(Colord::black(),
                       f.material.shade(f.raytracer.get(), *f.scene, f.ray, f.hitPoint, f.state),
                       0.001);
  }

  TEST(MatteMaterial, ShouldReturnBlackWhenTextureIsNull) {
    ShadeFixture f;
    f.material.setDiffuseTexture(nullptr);
    f.scene->setAmbient(Colord::white());
    f.scene->addLight(std::make_shared<DirectionalLight>(
      Vector3d(0, -1, 0), Colord(1, 1, 1)));

    // No texture → texColor is black → both ambient and diffuse terms are 0.
    ASSERT_COLOR_NEAR(Colord::black(),
                       f.material.shade(f.raytracer.get(), *f.scene, f.ray, f.hitPoint, f.state),
                       0.001);
  }
}
