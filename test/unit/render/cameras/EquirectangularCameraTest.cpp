#include <gtest/gtest.h>

#include "render/cameras/EquirectangularCamera.h"
#include "engine/raytracer/Raytracer.h"
#include "render/primitives/Scene.h"
#include "core/Buffer.h"
#include "core/math/Constants.h"

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
