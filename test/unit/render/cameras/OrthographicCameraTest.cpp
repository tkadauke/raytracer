#include <gtest/gtest.h>
#include "render/cameras/OrthographicCamera.h"
#include "engine/raytracer/Raytracer.h"
#include "render/animation/AnimationTrack.h"
#include "render/primitives/Scene.h"
#include "render/samplers/Sampler.h"
#include "core/Buffer.h"

#include "test/helpers/CameraTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

namespace OrthographicCameraTest {
  using namespace ::testing;
  using namespace render;
  using namespace engine::raytracer;
  using test::setupViewPlane;

  Vector3d descriptorDirection(const GpuPrimaryPathDescriptor& descriptor) {
    return Vector3d(descriptor.rectilinear.originOrDirection[0],
                    descriptor.rectilinear.originOrDirection[1],
                    descriptor.rectilinear.originOrDirection[2]);
  }

  Vector3d descriptorMotionOriginDelta(const GpuPrimaryPathDescriptor& descriptor) {
    return Vector3d(descriptor.rectilinear.motionOriginDelta[0],
                    descriptor.rectilinear.motionOriginDelta[1],
                    descriptor.rectilinear.motionOriginDelta[2]);
  }

  TEST(OrthographicCamera, ShouldConstructWithoutParameters) {
    OrthographicCamera camera;
  }

  TEST(OrthographicCamera, ShouldConstructWithParameters) {
    OrthographicCamera camera(Vector3d(0, 0, 1), Vector3d::null);
  }

  TEST(OrthographicCamera, ShouldRender) {
    OrthographicCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    auto scene = std::make_shared<Scene>(Colord::white());
    auto raytracer = std::make_shared<Raytracer>(scene);
    Buffer<Colord> buffer(1, 1);
    camera.render(raytracer, buffer);
    ASSERT_EQ(Colord::white(), buffer[0][0]);
  }

  TEST(OrthographicCamera, ShouldGetRayForPixelWithUninitializedViewPlane) {
    OrthographicCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    Rayd ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, 0), ray.origin());
    ASSERT_EQ(Vector3d(0, 0, 1), ray.direction());
  }

  TEST(OrthographicCamera, ShouldGetRayForPixelWithInitializedViewPlane) {
    OrthographicCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    auto raytracer = std::make_shared<Raytracer>(std::make_shared<Scene>(Colord::white()));
    Buffer<Colord> buffer(1, 1);
    camera.render(raytracer, buffer);

    Rayd ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, 0), ray.origin());
    ASSERT_EQ(Vector3d(0, 0, 1), ray.direction());
  }

  TEST(OrthographicCamera, PrimaryRayGeneratorSamplesAnimatedPose) {
    OrthographicCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    setupViewPlane(camera, 4, 3);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 1.0, -5.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(0.0, 1.0, 0.0)}}));

    auto pixel = camera.viewPlane()->pixelBegin(Recti(0, 0, 4, 3));
    render::NullSampleStream stream;
    const auto sample = camera.primaryRayGenerator()->sample(pixel, stream);

    auto expectedPlane = camera.viewPlane()->clone();
    expectedPlane->setup(
      Matrix4d::lookAt(Vector3d(0.0, 0.5, -5.0), Vector3d(0.0, 0.5, 0.0), Vector3d::up()),
      camera.viewPlane()->window());
    ASSERT_TRUE(sample);
    ASSERT_VECTOR_NEAR(expectedPlane->pixelAt(0.5, 0.5), Vector3d(sample->ray.origin()), 1e-9);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 1.0), sample->ray.direction(), 1e-9);
    EXPECT_DOUBLE_EQ(0.5, sample->timeSample);
    EXPECT_DOUBLE_EQ(0.5, sample->animationTime);
  }

  TEST(OrthographicCamera, ShouldExposeFixedShutterAnimatedGpuPrimaryPathDescriptor) {
    OrthographicCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    setupViewPlane(camera, 4, 3);
    camera.viewPlane()->sampler()->setup(1, 1, 17);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.25, 0.25);
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(1.0, 0.0, 0.0)}}));

    const auto descriptor = camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 3), 1234);

    ASSERT_TRUE(descriptor);
    EXPECT_EQ(gpuPrimaryPathGenerationModeOrthographic, descriptor->mode);
    const Vector3d expectedDirection =
      Matrix4d::lookAt(Vector3d(0.0, 0.0, -5.0), Vector3d(0.25, 0.0, 0.0), Vector3d::up())
        .transformDirection(Vector3d::forward())
        .normalized();
    ASSERT_VECTOR_NEAR(expectedDirection, descriptorDirection(*descriptor), 1e-6);
    EXPECT_EQ(12u, descriptor->pathCount());
  }

  TEST(OrthographicCamera, ShouldExposeSampledShutterRigTranslationGpuPrimaryPathDescriptor) {
    OrthographicCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    setupViewPlane(camera, 4, 3);
    camera.viewPlane()->sampler()->setup(1, 1, 17);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 1.0, -5.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(0.0, 1.0, 0.0)}}));

    const auto descriptor = camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 3), 1234);

    ASSERT_TRUE(descriptor);
    EXPECT_EQ(gpuPrimaryPathGenerationModeOrthographic, descriptor->mode);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 1.0), descriptorDirection(*descriptor), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 1.0, 0.0), descriptorMotionOriginDelta(*descriptor), 1e-6);
    EXPECT_EQ(12u, descriptor->pathCount());
  }

  TEST(OrthographicCamera, ShouldRejectSampledShutterRotatingGpuPrimaryPathDescriptor) {
    OrthographicCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    setupViewPlane(camera, 4, 3);
    camera.viewPlane()->sampler()->setup(1, 1, 17);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(1.0, 0.0, 0.0)}}));

    EXPECT_FALSE(camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 3), 1234));
  }

  TEST(OrthographicCamera, ClipSpaceProjectionMatchesProjectionWithDepth) {
    OrthographicCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    setupViewPlane(camera, 200, 150);

    const Vector3d point(1.5, -0.5, 4.0);
    const Vector4d clip = camera.projectPointToClipSpace(point);
    const Vector3d projected = camera.projectPointWithDepth(point);
    const Vector3d fromClip = camera.viewPlane()->screenFromClipUnchecked(clip);

    ASSERT_FALSE(clip.isUndefined());
    EXPECT_NEAR(projected.x(), fromClip.x(), 1e-9);
    EXPECT_NEAR(projected.y(), fromClip.y(), 1e-9);
    EXPECT_NEAR(projected.z(), fromClip.z(), 1e-9);
  }

  TEST(OrthographicCamera, ClipSpaceProjectionUsesUnitPerspectiveDivisor) {
    OrthographicCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    setupViewPlane(camera);

    const Vector4d inFront = camera.projectPointToClipSpace(Vector3d::null);
    const Vector4d behind = camera.projectPointToClipSpace(Vector3d(0, 0, -2));

    EXPECT_FALSE(inFront.isUndefined());
    EXPECT_FALSE(behind.isUndefined());
    EXPECT_EQ(1.0, inFront.w());
    EXPECT_EQ(1.0, behind.w());
    EXPECT_GT(inFront.z(), 0.0);
    EXPECT_LT(behind.z(), 0.0);
  }

  TEST(OrthographicCamera, ProjectionMatrixMapsViewPlaneEdgeToNdcOne) {
    // projectionMatrix() is built via Matrix4::orthographic; verify the
    // canonical property: a point at the right edge of the view plane
    // (x = halfW) should project to NDC x = +1.
    OrthographicCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    setupViewPlane(camera, 200, 150);

    auto plane = camera.viewPlane();
    const double halfW = plane->hSpan() * plane->pixelSize() / 2.0;

    const Matrix4d m = camera.projectionMatrix();
    const Vector4d v = m * Vector4d(halfW, 0, 0, 1.0);
    EXPECT_NEAR(1.0, v.x(), 1e-9);
  }

  TEST(OrthographicCamera, ProjectionMatrixIsConsistentWithProjectPointToClipSpace) {
    // The x and y components of projectPointToClipSpace must match the x and y
    // produced by applying projectionMatrix() directly.
    OrthographicCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    setupViewPlane(camera, 200, 150);

    const Vector3d worldPoint(1.5, -0.5, 4.0);
    const Vector4d clip = camera.projectPointToClipSpace(worldPoint);

    const Vector3d pCam = camera.inverseMatrix() * Vector4d(worldPoint);
    const Vector4d v = camera.projectionMatrix() * Vector4d(pCam.x(), pCam.y(), pCam.z(), 1.0);

    ASSERT_FALSE(clip.isUndefined());
    EXPECT_NEAR(v.x(), clip.x(), 1e-9);
    EXPECT_NEAR(v.y(), clip.y(), 1e-9);
  }
}
