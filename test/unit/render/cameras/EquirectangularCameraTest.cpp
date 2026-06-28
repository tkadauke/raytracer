#include <gtest/gtest.h>

#include <optional>

#include "render/GpuPrimaryPathDescriptor.h"
#include "render/cameras/EquirectangularCamera.h"
#include "engine/raytracer/Raytracer.h"
#include "render/primitives/Scene.h"
#include "render/samplers/Sampler.h"
#include "core/Buffer.h"
#include "core/math/Constants.h"
#include "render/animation/AnimationTrack.h"

#include "test/helpers/CameraTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

namespace EquirectangularCameraTest {
  using namespace render;
  using namespace engine::raytracer;
  using test::setupViewPlane;

  TEST(EquirectangularCamera, ShouldConstructWithoutParameters) {
    EquirectangularCamera camera;
  }

  TEST(EquirectangularCamera, ShouldConstructWithParameters) {
    EquirectangularCamera camera(Vector3d(0, 0, 1), Vector3d::null);
  }

  TEST(EquirectangularCamera, ShouldOriginateRayAtCameraPosition) {
    EquirectangularCamera camera(Vector3d(1, 2, 3), Vector3d::null);
    setupViewPlane(camera, 4, 2);

    auto ray = camera.rayForPixel(2, 1);
    ASSERT_VECTOR_NEAR(Vector3d(1, 2, 3), Vector3d(ray.origin()), 1e-9);
  }

  TEST(EquirectangularCamera, ShouldMapImageCentreToForward) {
    // Camera at origin pointing toward +z. The pixel at the image
    // centre (lon=0, lat=0) should yield a ray in the +z direction.
    EquirectangularCamera camera(Vector3d::null, Vector3d(0, 0, 1));
    setupViewPlane(camera, 360, 180);

    auto ray = camera.rayForPixel(180, 90);
    ASSERT_VECTOR_NEAR(Vector3d(0, 0, 1), ray.direction(), 1e-3);
  }

  TEST(EquirectangularCamera, ShouldMapTopEdgeToNorthPole) {
    // Top row of the equirect image is the north pole — direction must
    // point along the camera's local "up" axis. In this codebase
    // `Vector3d::up() == (0, -1, 0)`, so the north-pole direction is
    // (0, -1, 0). Pin so a future refactor that drops the y-flip in
    // EquirectangularCamera::direction silently produces an upside-
    // down panorama (floor at top, sky at bottom).
    EquirectangularCamera camera(Vector3d::null, Vector3d(0, 0, 1));
    setupViewPlane(camera, 360, 180);

    auto ray = camera.rayForPixel(180, 0);
    ASSERT_VECTOR_NEAR(Vector3d(0, -1, 0), ray.direction(), 1e-3);
  }

  TEST(EquirectangularCamera, ShouldMapBottomEdgeToSouthPole) {
    // Bottom row → south pole = "down" = world +y in this codebase.
    EquirectangularCamera camera(Vector3d::null, Vector3d(0, 0, 1));
    setupViewPlane(camera, 360, 180);

    auto ray = camera.rayForPixel(180, 180);
    ASSERT_VECTOR_NEAR(Vector3d(0, 1, 0), ray.direction(), 1e-3);
  }

  TEST(EquirectangularCamera, ShouldProduceUnitLengthDirections) {
    // Sphere parameterisation always yields unit vectors; pin so a
    // future refactor that drops the cos(lat) factor (turning the
    // mapping into a cylindrical projection) trips a test.
    EquirectangularCamera camera(Vector3d::null, Vector3d(0, 0, 1));
    setupViewPlane(camera, 360, 180);

    for (int x : {0, 90, 180, 270}) {
      for (int y : {0, 45, 90, 135, 180}) {
        auto ray = camera.rayForPixel(x, y);
        EXPECT_NEAR(1.0, ray.direction().length(), 1e-9);
      }
    }
  }

  TEST(EquirectangularCamera, ShouldExposeStaticGpuPrimaryPathDescriptor) {
    EquirectangularCamera camera(Vector3d(1, 2, 3), Vector3d(1, 2, 4));
    setupViewPlane(camera, 4, 2);
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    const std::optional<GpuPrimaryPathDescriptor> descriptor =
      camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 2), 1234);

    ASSERT_TRUE(descriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeEquirectangular, descriptor->mode);
    EXPECT_EQ(0, descriptor->requestedRect().left());
    EXPECT_EQ(0, descriptor->requestedRect().top());
    EXPECT_EQ(4, descriptor->requestedRect().width());
    EXPECT_EQ(2, descriptor->requestedRect().height());
    EXPECT_EQ(0, descriptor->actualRect().left());
    EXPECT_EQ(0, descriptor->actualRect().top());
    EXPECT_EQ(4, descriptor->actualRect().width());
    EXPECT_EQ(2, descriptor->actualRect().height());
    EXPECT_EQ(32u, descriptor->pathCount());

    const GpuRectilinearPrimaryPathDescriptor& payload = descriptor->rectilinear;
    EXPECT_EQ(4u, payload.requestedWidth);
    EXPECT_EQ(2u, payload.requestedHeight);
    EXPECT_EQ(4u, payload.actualWidth);
    EXPECT_EQ(2u, payload.actualHeight);
    EXPECT_EQ(4u, payload.samplesPerPixel);
    EXPECT_EQ(1234u, payload.sampleSeed);
    EXPECT_FLOAT_EQ(4.0f, payload.lensParameters[0]);
    EXPECT_FLOAT_EQ(2.0f, payload.lensParameters[1]);
    EXPECT_FLOAT_EQ(1.0f, payload.originOrDirection[0]);
    EXPECT_FLOAT_EQ(2.0f, payload.originOrDirection[1]);
    EXPECT_FLOAT_EQ(3.0f, payload.originOrDirection[2]);
    EXPECT_FLOAT_EQ(1.0f, payload.originOrDirection[3]);
  }

  TEST(EquirectangularCamera, ShouldExposeSampledShutterRigTranslationGpuPrimaryPathDescriptor) {
    EquirectangularCamera camera(Vector3d(0, 0, -5), Vector3d(0, 0, -4));
    setupViewPlane(camera, 4, 2);
    camera.viewPlane()->sampler()->setup(1, 1, 17);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 1.0, -5.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -4.0)}, {1.0, Vector3d(0.0, 1.0, -4.0)}}));

    const std::optional<GpuPrimaryPathDescriptor> descriptor =
      camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 2), 1234);

    ASSERT_TRUE(descriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeEquirectangular, descriptor->mode);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -5.0),
                       Vector3d(descriptor->rectilinear.originOrDirection), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 1.0, 0.0), Vector3d(descriptor->rectilinear.motionOriginDelta),
                       1e-6);
    EXPECT_EQ(8u, descriptor->pathCount());
  }

  TEST(EquirectangularCamera, ShouldRenderViaRaytracer) {
    // End-to-end: route through Raytracer::render which sets up the
    // viewplane. The full sphere wraps a white scene → every pixel
    // should land white.
    EquirectangularCamera camera(Vector3d::null, Vector3d(0, 0, 1));
    auto scene = std::make_shared<Scene>(Colord::white());
    Buffer<Colord> buffer(4, 2);
    // Set up the viewplane manually because the test isn't going
    // through Raytracer (which would do it automatically).
    setupViewPlane(camera, 4, 2);
    auto raytracer = std::make_shared<Raytracer>(scene);
    camera.render(raytracer, buffer);
    EXPECT_EQ(Colord::white(), buffer[0][0]);
  }
}
