#include <gtest/gtest.h>
#include "raytracer/materials/TransparentMaterial.h"
#include "render/textures/ConstantColorTexture.h"
#include "raytracer/Raytracer.h"
#include "raytracer/State.h"
#include "raytracer/primitives/Scene.h"
#include "core/math/Ray.h"
#include "core/math/HitPoint.h"

#include <cmath>

#include "core/math/HitPoint.h"
#include "raytracer/Raytracer.h"
#include "raytracer/State.h"
#include "raytracer/primitives/Scene.h"

#include "test/helpers/ColorTestHelper.h"

namespace TransparentMaterialTest {
  using namespace raytracer;
using namespace render;

  TEST(TransparentMaterial, ShouldInitialize) {
    TransparentMaterial material;
    ASSERT_EQ(Colord::white(), material.specularColor());
    ASSERT_EQ(1, material.refractionIndex());
  }

  TEST(TransparentMaterial, ShouldInitializeWithDiffuseTexture) {
    auto texture = std::make_shared<ConstantColorTexture>(Colord(0, 1, 0));
    TransparentMaterial material(texture);
    ASSERT_EQ(texture, material.diffuseTexture());
    ASSERT_EQ(Colord::white(), material.specularColor());
    ASSERT_EQ(1, material.refractionIndex());
  }

  TEST(TransparentMaterial, ShouldSetHighlightColor) {
    TransparentMaterial material;
    material.setSpecularColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.specularColor());
  }

  TEST(TransparentMaterial, ShouldSetRefractionIndex) {
    TransparentMaterial material;
    material.setRefractionIndex(1.4);
    ASSERT_EQ(1.4, material.refractionIndex());
  }

  TEST(TransparentMaterial, ShouldSetTransmissionCoefficient) {
    TransparentMaterial material;
    material.setTransmissionCoefficient(0.5);
    ASSERT_EQ(0.5, material.transmissionCoefficient());
  }

  TEST(TransparentMaterial, ShouldSetReflectionColor) {
    TransparentMaterial material;
    material.setReflectionColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.reflectionColor());
  }

  TEST(TransparentMaterial, ShouldSetReflectionCoefficient) {
    TransparentMaterial material;
    material.setReflectionCoefficient(0.5);
    ASSERT_EQ(0.5, material.reflectionCoefficient());
  }

  // ---- shading-behaviour tests ---------------------------------------------
  //
  // TransparentMaterial::shade has two branches:
  //
  //   if totalInternalReflection(ray, hitPoint):
  //       return rayColor(reflected)         // pure mirror, no TIR scaling
  //   else:
  //       return PhongMaterial::shade(...)
  //            + reflectionColor * coeff * rayColor(reflected) * |N·in|
  //            + transmittedColor * rayColor(transmitted)        * |N·trans|
  //
  // (The /|N·in| inside PerfectSpecular and PerfectTransmitter cancels the
  // |N·in|/|N·trans| in shade, so each contribution simplifies to
  // reflectionColor * coeff * traced and transmittedColor * traced.)

  namespace {
    struct ShadeFixture {
      std::shared_ptr<ConstantColorTexture> texture =
        std::make_shared<ConstantColorTexture>(Colord::white());
      std::shared_ptr<Scene> scene = std::make_shared<Scene>(Colord::black());
      std::shared_ptr<Raytracer> raytracer = std::make_shared<Raytracer>(scene);

      HitPoint hitPoint{nullptr, 1.0, Vector4d(0, 0, 0, 1), Vector3d(0, 1, 0)};
      Rayd ray{Vector3d(0, 5, 0), Vector3d(0, -1, 0)};
      State state;

      TransparentMaterial material{texture};

      ShadeFixture() {
        // Suppress the inherited Phong term so the reflection / transmission
        // contributions we're testing here aren't tangled with ambient or
        // diffuse output.
        scene->setAmbient(Colord::black());
        material.setAmbientCoefficient(0.0);
        material.setDiffuseCoefficient(0.0);
        material.setSpecularCoefficient(0.0);
        // Defaults that each test will tune:
        material.setRefractionIndex(1.0);
        material.setReflectionCoefficient(0.0);
        material.setTransmissionCoefficient(1.0);
      }
    };
  }

  TEST(TransparentMaterial, ShouldTransmitBackgroundUnchangedAtIndexOne) {
    ShadeFixture f;
    // index = 1 means the refracted ray continues straight through the
    // surface; with no other primitives in the scene it hits the
    // background. transmittedColor at index 1 is white * 1 / 1 / |N·trans|,
    // and the |N·trans| in shade cancels it back to white * background.
    f.scene->setBackground(Colord(1, 0, 0));

    auto colour = f.material.shade(f.raytracer.get(), f.ray, f.hitPoint, f.state);

    ASSERT_COLOR_NEAR(Colord(1, 0, 0), colour, 0.001);
  }

  TEST(TransparentMaterial, ShouldAddReflectionContribution) {
    ShadeFixture f;
    f.scene->setBackground(Colord(0, 1, 0));
    f.material.setReflectionColor(Colord::white());
    f.material.setReflectionCoefficient(0.5);
    f.material.setTransmissionCoefficient(0.0);
    // Transmission contribution still fires (transmitted ray exists), but
    // with transmissionCoefficient 0 the transmittedColor is black.
    // Result is just the reflection branch: 0.5 * 1 * green = (0, 0.5, 0).

    auto colour = f.material.shade(f.raytracer.get(), f.ray, f.hitPoint, f.state);

    ASSERT_COLOR_NEAR(Colord(0, 0.5, 0), colour, 0.001);
  }

  TEST(TransparentMaterial, ShouldSumReflectionAndTransmission) {
    ShadeFixture f;
    f.scene->setBackground(Colord(1, 1, 1));
    f.material.setReflectionColor(Colord::white());
    f.material.setReflectionCoefficient(0.25);
    f.material.setTransmissionCoefficient(0.5);
    // Both branches see the same white background:
    //   reflection    = 0.25 * 1 * (1,1,1) = (0.25, 0.25, 0.25)
    //   transmission  = 0.5  * 1 * (1,1,1) = (0.5,  0.5,  0.5)
    // sum = (0.75, 0.75, 0.75).

    auto colour = f.material.shade(f.raytracer.get(), f.ray, f.hitPoint, f.state);

    ASSERT_COLOR_NEAR(Colord(0.75, 0.75, 0.75), colour, 0.001);
  }

  TEST(TransparentMaterial, ShouldReturnPureReflectionUnderTotalInternalReflection) {
    ShadeFixture f;
    // refractionIndex < 1 lets TIR fire on a sufficiently-grazing ray.
    // Setup: refractionIndex 0.5 + 45° incidence: N·out = cos 45° ≈ 0.707;
    // 1 - (1 - 0.5)/0.25 = -1 < 0 → TIR. The shade method then returns
    // rayColor(reflected) added on top of the (zeroed-out) Phong term.
    // The fixture suppresses ambient/diffuse/specular so the Phong
    // contribution is black and the result is just the reflected ray.
    f.material.setRefractionIndex(0.5);
    constexpr double s = 0.7071067811865476;
    f.ray = Rayd(Vector3d(5, 5, 0), Vector3d(-s, -s, 0));
    f.scene->setBackground(Colord(1, 0, 0));

    auto colour = f.material.shade(f.raytracer.get(), f.ray, f.hitPoint, f.state);

    // Reflected ray goes back up into empty scene → background.
    ASSERT_COLOR_NEAR(Colord(1, 0, 0), colour, 0.001);
  }

  // Regression for the bug fixed in #36: prior to the fix, the
  // total-internal-reflection branch of TransparentMaterial::shade returned
  // *only* the recursive reflected-ray colour, dropping the Phong direct
  // lighting contribution. Deep TIR chains in glass (the canonical case
  // being a torus or dense interior geometry) therefore lost all their
  // diffuse + ambient + specular character and rendered as mirror reflection
  // alone, often appearing as black voids when the reflection bottomed out
  // at the depth limit.
  //
  // The fix moves the PhongMaterial::shade() call ahead of the TIR check
  // and *adds* the recursive reflection on top, so direct lighting is
  // preserved on both branches.
  //
  // This test sets up a TIR-triggering hit (ray exiting glass at 60° from
  // the outward normal, IOR 1.5; critical angle is ~41.8°), forces the
  // recursive reflection to truncate immediately to a black scene
  // background (max recursion depth = 1), and asserts that the result is
  // *not* black — proving direct lighting was added.
  TEST(TransparentMaterial, ShouldIncludeDirectLightingOnTIRBranch) {
    auto scene = std::make_shared<Scene>();
    scene->setAmbient(Colord::white());
    scene->setBackground(Colord::black());

    auto material = std::make_shared<TransparentMaterial>(
        std::make_shared<ConstantColorTexture>(Colord(0.7, 0.2, 0.1)));
    material->setRefractionIndex(1.5);

    auto rt = std::make_shared<Raytracer>(scene);
    rt->setMaximumRecursionDepth(1);  // forces the reflected ray to truncate.

    // Outward normal +y; ray exiting glass at 60° from normal — past the
    // ~41.8° critical angle for IOR 1.5, so TIR triggers.
    const double angle = 60.0 * M_PI / 180.0;
    Vector3d direction(std::sin(angle), std::cos(angle), 0);
    Rayd ray(Vector3d(0, -1, 0), direction);
    HitPoint hitPoint(nullptr, 1.0, Vector4d(0, 0, 0, 1), Vector3d(0, 1, 0));

    State state;
    state.startTrace();
    Colord result = material->shade(rt.get(), ray, hitPoint, state);

    // Verify TIR actually fired (so we know we're testing the right branch).
    bool tirEventLogged = false;
    for (const auto& event : *state.events) {
      if (event.find("TIR") != std::string::npos) {
        tirEventLogged = true;
        break;
      }
    }
    ASSERT_TRUE(tirEventLogged) << "expected the TIR branch to execute";

    // Phong ambient contribution = texColor (red-ish) * scene.ambient (white).
    // Pre-fix this would have been Colord::black() (truncated reflection).
    ASSERT_NE(Colord::black(), result);
    ASSERT_GT(result.r(), 0.0);
  }
}
