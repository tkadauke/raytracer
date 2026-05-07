#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "raytracer/Raytracer.h"
#include "raytracer/State.h"
#include "render/cameras/PinholeCamera.h"
#include "render/lights/DirectionalLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"

#include "core/Buffer.h"
#include "core/math/BoundingBox.h"
#include "core/math/HitPoint.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

#include "test/helpers/ColorTestHelper.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace RaytracerTest {
  using namespace ::testing;
  using namespace raytracer;
using namespace render;

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

    // Builds a NiceMock<MockPrimitive> that always reports a hit at the given
    // distance along the ray, with an outward-facing normal. Used by tests
    // that need rayColor() to enter the "primitive hit" branch without
    // bringing a real geometric primitive (and its bounding-box accessors)
    // into scope.
    std::shared_ptr<NiceMock<MockPrimitive>> makeAlwaysHit(double distance = 1.0) {
      auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
      BoundingBoxd bbox(Vector3d(-100, -100, -100), Vector3d(100, 100, 100));
      HitPoint hit(primitive.get(), distance,
                   Vector4d(0, 0, distance, 1), Vector3d(0, 0, -1));
      ON_CALL(*primitive, calculateBoundingBox()).WillByDefault(Return(bbox));
      ON_CALL(*primitive, intersect(_, _, _))
        .WillByDefault(DoAll(AddHitPoint(hit), Return(primitive.get())));
      return primitive;
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

  TEST(Raytracer, RayColorShouldReturnBackgroundAtMaximumRecursionDepth) {
    // Regression for #35: previously truncation returned Colord::black(),
    // which made deep TIR chains in glass tori render as black voids.
    // After the fix, truncation falls through to the scene background —
    // a softer fallback that matches what a primary miss would see.
    auto scene = std::make_shared<Scene>();
    scene->setBackground(Colord(0.7, 0.4, 0.1));
    Raytracer raytracer(scene);
    // rayColor's first action is state.recurseIn() (state starts at 0 → 1),
    // then it checks `if (recursionDepth == maximumRecursionDepth)`. Setting
    // max to 1 makes the very first call short-circuit, never reaching the
    // scene intersection.
    raytracer.setMaximumRecursionDepth(1);

    State state;
    Rayd ray(Vector3d(0, 0, -5), Vector3d(0, 0, 1));
    auto colour = raytracer.rayColor(ray, state);

    ASSERT_COLOR_NEAR(scene->background(), colour, 1e-9);
  }

  TEST(Raytracer, RayColorShouldShortCircuitBeforeIntersectAtMaximumDepth) {
    // Even with a hittable primitive in the scene, hitting the depth
    // limit must short-circuit before the intersect call is ever made.
    // Mock-based variant of the test above — pins the *order* of the
    // depth check vs the intersect dispatch.
    auto scene = std::make_shared<Scene>();
    scene->setBackground(Colord(0.7, 0.4, 0.1));
    scene->add(makeAlwaysHit());
    Raytracer raytracer(scene);
    raytracer.setMaximumRecursionDepth(1);

    State state;
    Rayd ray(Vector3d(0, 0, 0), Vector3d(0, 0, 1));
    ASSERT_EQ(scene->background(), raytracer.rayColor(ray, state));
  }

  TEST(Raytracer, RayColorShouldRestoreRecursionDepthAfterReturning) {
    // The recurseIn / recurseOut pair around rayColor must balance regardless
    // of which return path is taken.
    auto scene = std::make_shared<Scene>();
    Raytracer raytracer(scene);
    Rayd ray(Vector3d(0, 0, 0), Vector3d(0, 0, 1));
    State state;
    raytracer.rayColor(ray, state);
    ASSERT_EQ(0, state.recursionDepth);
  }

  TEST(Raytracer, RayColorShouldTrackMaxRecursionDepthInState) {
    // One non-recursive call records a max depth of 1.
    auto scene = std::make_shared<Scene>();
    Raytracer raytracer(scene);
    Rayd ray(Vector3d(0, 0, 0), Vector3d(0, 0, 1));
    State state;
    raytracer.rayColor(ray, state);
    ASSERT_EQ(1, state.maxRecursionDepth);
  }

  TEST(Raytracer, RayColorShouldRespectCustomMaximumRecursionDepth) {
    // setMaximumRecursionDepth(N) must control where the depth check fires.
    // Pre-loading state.recursionDepth simulates having already recursed
    // N-1 times; the next rayColor call should hit the limit.
    auto scene = std::make_shared<Scene>();
    scene->setBackground(Colord(0.1, 0.2, 0.3));
    Raytracer raytracer(scene);
    raytracer.setMaximumRecursionDepth(5);

    Rayd ray(Vector3d(0, 0, 0), Vector3d(0, 0, 1));
    State state;
    state.recursionDepth = 4;  // recurseIn() will make it 5, hitting the limit.
    ASSERT_EQ(scene->background(), raytracer.rayColor(ray, state));
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
    Raytracer raytracer(std::shared_ptr<render::Scene>(nullptr));
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
