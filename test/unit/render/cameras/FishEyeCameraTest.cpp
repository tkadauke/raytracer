#include <gtest/gtest.h>
#include "render/cameras/FishEyeCamera.h"
#include "engine/raytracer/Raytracer.h"
#include "render/animation/AnimationTrack.h"
#include "render/samplers/Sampler.h"
#include "render/viewplanes/ViewPlane.h"
#include "render/primitives/Scene.h"
#include "core/Buffer.h"

#include "test/helpers/VectorTestHelper.h"

#include <optional>

namespace FishEyeCameraTest {
  using namespace ::testing;
  using namespace render;
  using namespace engine::raytracer;

  TEST(FishEyeCamera, ShouldConstructWithoutParameters) {
    FishEyeCamera camera;
    ASSERT_NEAR(120, camera.fieldOfView().degrees(), 0.001);
  }

  TEST(FishEyeCamera, ShouldConstructWithParameters) {
    FishEyeCamera camera(Vector3d(0, 0, 1), Vector3d::null);
    ASSERT_NEAR(120, camera.fieldOfView().degrees(), 0.001);
  }

  TEST(FishEyeCamera, ShouldConstructWithFieldOfView) {
    FishEyeCamera camera(360_degrees);
    ASSERT_NEAR(360, camera.fieldOfView().degrees(), 0.001);
  }

  TEST(FishEyeCamera, ShouldSetFieldOfView) {
    FishEyeCamera camera;
    camera.setFieldOfView(200_degrees);
    ASSERT_NEAR(200, camera.fieldOfView().degrees(), 0.001);
  }

  TEST(FishEyeCamera, ShouldRender) {
    FishEyeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    auto scene = std::make_shared<Scene>(Colord::white());
    auto raytracer = std::make_shared<Raytracer>(scene);
    Buffer<Colord> buffer(1, 1);
    camera.render(raytracer, buffer);
    // This is black because of the black rounded border around fish eye images
    ASSERT_EQ(Colord::black(), buffer[0][0]);
  }

  TEST(FishEyeCamera, ShouldGetRayForPixelWithUninitializedViewPlane) {
    FishEyeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    Rayd ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -1), ray.origin());
    ASSERT_TRUE(ray.direction().isUndefined());
  }

  TEST(FishEyeCamera, ShouldGetRayForPixelWithInitializedViewPlane) {
    FishEyeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    auto raytracer = std::make_shared<Raytracer>(std::make_shared<Scene>(Colord::white()));
    Buffer<Colord> buffer(1, 1);
    camera.render(raytracer, buffer);

    Rayd ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -1), ray.origin());
    ASSERT_TRUE(ray.direction().isUndefined());
  }

  TEST(FishEyeCamera, ShouldExposeStaticGpuPrimaryPathDescriptor) {
    FishEyeCamera camera(Vector3d(1, 2, 3), Vector3d(1, 2, 4));
    camera.setFieldOfView(180_degrees);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 4));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    const std::optional<GpuPrimaryPathDescriptor> descriptor =
      camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 4), 1234);

    ASSERT_TRUE(descriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeFishEye, descriptor->mode);
    EXPECT_TRUE(descriptor->generatesOnDevice());
    EXPECT_EQ(64u, descriptor->pathCount());

    const GpuRectilinearPrimaryPathDescriptor& rectilinear = descriptor->rectilinear;
    EXPECT_FLOAT_EQ(1.0f, rectilinear.originOrDirection[0]);
    EXPECT_FLOAT_EQ(2.0f, rectilinear.originOrDirection[1]);
    EXPECT_FLOAT_EQ(3.0f, rectilinear.originOrDirection[2]);
    EXPECT_FLOAT_EQ(1.0f, rectilinear.originOrDirection[3]);
    EXPECT_FLOAT_EQ(4.0f, rectilinear.lensParameters[0]);
    EXPECT_FLOAT_EQ(4.0f, rectilinear.lensParameters[1]);
    EXPECT_FLOAT_EQ(static_cast<float>((180_degrees).radians()), rectilinear.lensParameters[2]);
    EXPECT_FLOAT_EQ(0.0f, rectilinear.lensParameters[3]);
    EXPECT_EQ(0, rectilinear.requestedLeft);
    EXPECT_EQ(0, rectilinear.requestedTop);
    EXPECT_EQ(4u, rectilinear.requestedWidth);
    EXPECT_EQ(4u, rectilinear.requestedHeight);
    EXPECT_EQ(0, rectilinear.actualLeft);
    EXPECT_EQ(0, rectilinear.actualTop);
    EXPECT_EQ(4u, rectilinear.actualWidth);
    EXPECT_EQ(4u, rectilinear.actualHeight);
    EXPECT_EQ(4u, rectilinear.samplesPerPixel);
    EXPECT_EQ(1234u, rectilinear.sampleSeed);
  }

  TEST(FishEyeCamera, ShouldExposeSampledShutterRigTranslationGpuPrimaryPathDescriptor) {
    FishEyeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.setFieldOfView(180_degrees);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 4));
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
      camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 4), 1234);

    ASSERT_TRUE(descriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeFishEye, descriptor->mode);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -5.0),
                       Vector3d(descriptor->rectilinear.originOrDirection), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 1.0, 0.0), Vector3d(descriptor->rectilinear.motionOriginDelta),
                       1e-6);
    EXPECT_EQ(16u, descriptor->pathCount());
  }
}
