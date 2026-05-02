#include <gtest/gtest.h>

#include "raytracer/Raytracer.h"
#include "raytracer/State.h"
#include "raytracer/cameras/PinholeCamera.h"
#include "raytracer/lights/DirectionalLight.h"
#include "raytracer/materials/MatteMaterial.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/primitives/Sphere.h"
#include "raytracer/textures/ConstantColorTexture.h"

#include "core/Buffer.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

#include "test/helpers/ColorTestHelper.h"

namespace RaytracerTest {
  using namespace raytracer;

  // Tests for the orchestration class itself (issue #20). render() is not
  // exercised here because it spins up a QThreadPool; that path is covered
  // indirectly by the functional suite. The tests below focus on
  // rayColor/rayState/primitiveForRay and the recursion-depth invariant —
  // pure logic that runs without any threading or Qt machinery.

  namespace {
    // Build a sphere of `radius` at the origin with a unit-white matte
    // material — the simplest "primitive that shades to a known colour".
    std::shared_ptr<Sphere> whiteSphere(double radius) {
      auto sphere = std::make_shared<Sphere>(Vector3d::null(), radius);
      sphere->setMaterial(std::make_shared<MatteMaterial>(
        std::make_shared<ConstantColorTexture>(Colord::white())));
      return sphere;
    }
  }

  TEST(Raytracer, ShouldDefaultToPinholeCameraWhenConstructedWithSceneOnly) {
    auto scene = std::make_shared<Scene>(Colord::black());
    Raytracer raytracer(scene);
    ASSERT_TRUE(std::dynamic_pointer_cast<PinholeCamera>(raytracer.camera()));
  }

  TEST(Raytracer, ShouldUseExplicitCameraWhenProvided) {
    auto scene = std::make_shared<Scene>(Colord::black());
    auto camera = std::make_shared<PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null());
    Raytracer raytracer(camera, scene);
    ASSERT_EQ(camera, raytracer.camera());
  }

  TEST(Raytracer, ShouldExposeAndUpdateScene) {
    auto first  = std::make_shared<Scene>(Colord::black());
    auto second = std::make_shared<Scene>(Colord::white());
    Raytracer raytracer(first);
    ASSERT_EQ(first, raytracer.scene());

    raytracer.setScene(second);
    ASSERT_EQ(second, raytracer.scene());
  }

  TEST(Raytracer, RayColorShouldReturnSceneBackgroundWhenRayMissesEverything) {
    auto scene = std::make_shared<Scene>();
    scene->setBackground(Colord(0.25, 0.5, 0.75));
    Raytracer raytracer(scene);

    State state;
    Rayd ray(Vector3d(0, 0, -10), Vector3d(0, 1, 0));  // straight up, hits nothing
    auto colour = raytracer.rayColor(ray, state);

    ASSERT_COLOR_NEAR(Colord(0.25, 0.5, 0.75), colour, 0.001);
  }

  TEST(Raytracer, RayColorShouldShadeMaterialOfHitPrimitive) {
    auto scene = std::make_shared<Scene>(Colord::black());
    scene->setAmbient(Colord::white());
    scene->add(whiteSphere(1.0));
    Raytracer raytracer(scene);

    State state;
    Rayd ray(Vector3d(0, 0, -5), Vector3d(0, 0, 1));
    auto colour = raytracer.rayColor(ray, state);

    // White ambient × white texture × MatteMaterial.ambientCoefficient (1)
    // = white. The actual numeric value is whatever Lambertian.reflectance
    // produces; we just need it to be non-black so we know shade ran.
    ASSERT_GT(colour[0], 0.0);
    ASSERT_GT(colour[1], 0.0);
    ASSERT_GT(colour[2], 0.0);
  }

  TEST(Raytracer, RayColorShouldReturnBlackWhenHitPrimitiveHasNoMaterial) {
    auto scene = std::make_shared<Scene>(Colord(1, 1, 1));
    auto sphere = std::make_shared<Sphere>(Vector3d::null(), 1.0);
    // Deliberately don't set a material on `sphere`.
    scene->add(sphere);
    Raytracer raytracer(scene);

    State state;
    Rayd ray(Vector3d(0, 0, -5), Vector3d(0, 0, 1));
    auto colour = raytracer.rayColor(ray, state);

    ASSERT_COLOR_NEAR(Colord::black(), colour, 1e-9);
  }

  TEST(Raytracer, RayColorShouldReturnBlackAtMaximumRecursionDepth) {
    auto scene = std::make_shared<Scene>();
    scene->setBackground(Colord(1, 0, 0));  // Red so a leaked-through return is obvious.
    Raytracer raytracer(scene);
    // rayColor's first action is state.recurseIn() (state starts at 0 → 1),
    // then it checks `if (recursionDepth == maximumRecursionDepth)`. Setting
    // max to 1 makes the very first call short-circuit to black, never
    // reaching the scene intersection or the background.
    raytracer.setMaximumRecursionDepth(1);

    State state;
    Rayd ray(Vector3d(0, 0, -5), Vector3d(0, 0, 1));
    auto colour = raytracer.rayColor(ray, state);

    ASSERT_COLOR_NEAR(Colord::black(), colour, 1e-9);
  }

  TEST(Raytracer, PrimitiveForRayShouldReturnHitPrimitive) {
    auto scene = std::make_shared<Scene>(Colord::black());
    auto sphere = whiteSphere(1.0);
    scene->add(sphere);
    Raytracer raytracer(scene);

    Rayd ray(Vector3d(0, 0, -5), Vector3d(0, 0, 1));
    auto hit = raytracer.primitiveForRay(ray);

    ASSERT_EQ(sphere.get(), hit);
  }

  TEST(Raytracer, PrimitiveForRayShouldReturnNullptrWhenRayMissesEverything) {
    auto scene = std::make_shared<Scene>(Colord::black());
    scene->add(whiteSphere(1.0));
    Raytracer raytracer(scene);

    Rayd ray(Vector3d(0, 0, -5), Vector3d(0, 1, 0));  // misses
    auto hit = raytracer.primitiveForRay(ray);

    ASSERT_EQ(nullptr, hit);
  }

  TEST(Raytracer, RayStateShouldPopulateHitPointAtRecursionDepthOne) {
    auto scene = std::make_shared<Scene>(Colord::black());
    auto sphere = whiteSphere(1.0);
    scene->add(sphere);
    Raytracer raytracer(scene);

    Rayd ray(Vector3d(0, 0, -5), Vector3d(0, 0, 1));
    auto state = raytracer.rayState(ray);

    // The hit on the front of the unit sphere is at z = -1.
    ASSERT_EQ(sphere.get(), state.hitPoint.primitive());
    ASSERT_NEAR(-1.0, state.hitPoint.point().z(), 1e-6);
  }

  TEST(Raytracer, RenderShouldClearBufferWhenSceneIsNullptr) {
    Raytracer raytracer(std::shared_ptr<Scene>(nullptr));
    Buffer<unsigned int> buffer(4, 4);
    // Pre-fill the buffer with a recognisable pattern so we can prove
    // render() actually cleared it.
    for (int y = 0; y < 4; ++y)
      for (int x = 0; x < 4; ++x)
        buffer[y][x] = 0xDEADBEEF;

    raytracer.render(buffer);

    for (int y = 0; y < 4; ++y)
      for (int x = 0; x < 4; ++x)
        ASSERT_EQ(0u, buffer[y][x]) << "at (" << x << "," << y << ")";
  }
}
