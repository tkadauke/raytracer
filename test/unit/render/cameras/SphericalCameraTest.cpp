#include <gtest/gtest.h>
#include "render/cameras/SphericalCamera.h"
#include "engine/raytracer/Raytracer.h"
#include "render/primitives/Scene.h"
#include "render/samplers/Sampler.h"
#include "core/Buffer.h"

#include "test/helpers/ImageViewer.h"

namespace SphericalCameraTest {
  using namespace ::testing;
  using namespace render;
  using namespace engine::raytracer;

  TEST(SphericalCamera, ShouldConstructWithoutParameters) {
    SphericalCamera camera;
    ASSERT_NEAR(180, camera.horizontalFieldOfView().degrees(), 0.001);
    ASSERT_NEAR(120, camera.verticalFieldOfView().degrees(), 0.001);
  }

  TEST(SphericalCamera, ShouldConstructWithParameters) {
    SphericalCamera camera(Vector3d(0, 0, 1), Vector3d::null);
    ASSERT_NEAR(180, camera.horizontalFieldOfView().degrees(), 0.001);
    ASSERT_NEAR(120, camera.verticalFieldOfView().degrees(), 0.001);
  }

  TEST(SphericalCamera, ShouldConstructWithFieldOfViews) {
    SphericalCamera camera(200_degrees, 90_degrees);
    ASSERT_NEAR(200, camera.horizontalFieldOfView().degrees(), 0.001);
    ASSERT_NEAR(90, camera.verticalFieldOfView().degrees(), 0.001);
  }

  TEST(SphericalCamera, ShouldSetHorizontalFieldOfView) {
    SphericalCamera camera;
    camera.setHorizontalFieldOfView(200_degrees);
    ASSERT_NEAR(200, camera.horizontalFieldOfView().degrees(), 0.001);
  }

  TEST(SphericalCamera, ShouldSetVerticalFieldOfView) {
    SphericalCamera camera;
    camera.setVerticalFieldOfView(140_degrees);
    ASSERT_NEAR(140, camera.verticalFieldOfView().degrees(), 0.001);
  }

  TEST(SphericalCamera, ShouldExposeStaticGpuPrimaryPathDescriptor) {
    SphericalCamera camera(Vector3d(1, 2, 3), Vector3d(1, 2, 4));
    camera.setFieldOfView(200_degrees, 90_degrees);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    const auto descriptor = camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 2), 1234);

    ASSERT_TRUE(descriptor.has_value());
    EXPECT_TRUE(descriptor->generatesOnDevice());
    EXPECT_EQ(gpuPrimaryPathGenerationModeSpherical, descriptor->mode);
    EXPECT_EQ(32u, descriptor->pathCount());

    const Recti requestedRect = descriptor->requestedRect();
    EXPECT_EQ(0, requestedRect.left());
    EXPECT_EQ(0, requestedRect.top());
    EXPECT_EQ(4, requestedRect.width());
    EXPECT_EQ(2, requestedRect.height());

    const Recti actualRect = descriptor->actualRect();
    EXPECT_EQ(0, actualRect.left());
    EXPECT_EQ(0, actualRect.top());
    EXPECT_EQ(4, actualRect.width());
    EXPECT_EQ(2, actualRect.height());

    EXPECT_EQ(camera.matrix().transformPoint(Vector3d(0, 0, -5)),
              Vector3d(descriptor->rectilinear.originOrDirection));
    EXPECT_FLOAT_EQ(4.0f, descriptor->rectilinear.lensParameters[0]);
    EXPECT_FLOAT_EQ(2.0f, descriptor->rectilinear.lensParameters[1]);
    EXPECT_FLOAT_EQ(static_cast<float>((200_degrees).radians()),
                    descriptor->rectilinear.lensParameters[2]);
    EXPECT_FLOAT_EQ(static_cast<float>((90_degrees).radians()),
                    descriptor->rectilinear.lensParameters[3]);
    EXPECT_EQ(4u, descriptor->rectilinear.samplesPerPixel);
    EXPECT_EQ(1234u, descriptor->rectilinear.sampleSeed);
  }

  TEST(SphericalCamera, ShouldRender) {
    auto camera = std::make_shared<SphericalCamera>(Vector3d(0, 0, -1), Vector3d::null);
    auto scene = std::make_shared<Scene>(Colord::white());
    auto raytracer = std::make_shared<Raytracer>(camera, scene);
    Buffer<Colord> buffer(1, 1);
    raytracer->render(buffer);
    ASSERT_EQ(Colord::white(), buffer[0][0]);
  }

  TEST(SphericalCamera, ShouldGetRayForPixelWithUninitializedViewPlane) {
    SphericalCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    Rayd ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -6), ray.origin());
    ASSERT_TRUE(ray.direction().isUndefined());
  }

  TEST(SphericalCamera, ShouldGetRayForPixelWithInitializedViewPlane) {
    SphericalCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    auto raytracer = std::make_shared<Raytracer>(std::make_shared<Scene>(Colord::white()));

    Rayd ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -6), ray.origin());
    ASSERT_TRUE(ray.direction().isUndefined());
  }
}
