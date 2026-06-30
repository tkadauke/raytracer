#include <gtest/gtest.h>

#include "render/cameras/PinholeCamera.h"
#include "render/cameras/TiltShiftCamera.h"
#include "engine/raytracer/Raytracer.h"
#include "render/animation/AnimationTrack.h"
#include "render/primitives/Scene.h"
#include "render/samplers/Sampler.h"

#include "test/helpers/CameraTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

#include <optional>

namespace TiltShiftCameraTest {
  using namespace render;
  using namespace engine::raytracer;
  using test::setupViewPlane;

  Vector3d descriptorOrigin(const GpuPrimaryPathDescriptor& descriptor) {
    return Vector3d(descriptor.rectilinear.originOrDirection);
  }

  Vector3d descriptorMotionOriginDelta(const GpuPrimaryPathDescriptor& descriptor) {
    return Vector3d(descriptor.rectilinear.motionOriginDelta);
  }

  Vector3d descriptorMotionTarget(const GpuPrimaryPathDescriptor& descriptor) {
    return Vector3d(descriptor.rectilinear.motionTarget);
  }

  Vector3d descriptorMotionTargetDelta(const GpuPrimaryPathDescriptor& descriptor) {
    return Vector3d(descriptor.rectilinear.motionTargetDelta);
  }

  TEST(TiltShiftCamera, ShouldDefaultToZeroTiltAndShift) {
    TiltShiftCamera camera;
    EXPECT_DOUBLE_EQ(0.0, camera.tilt().radians());
    EXPECT_DOUBLE_EQ(0.0, camera.shift().x());
    EXPECT_DOUBLE_EQ(0.0, camera.shift().y());
  }

  TEST(TiltShiftCamera, ShouldInheritThinLensDefaults) {
    // Pin the inherited defaults so a future refactor that drops the
    // ThinLens base would loudly break this test instead of silently
    // changing the canonical TiltShift starting state.
    TiltShiftCamera camera;
    EXPECT_DOUBLE_EQ(5.0, camera.distance());
    EXPECT_DOUBLE_EQ(0.1, camera.apertureRadius());
    EXPECT_DOUBLE_EQ(5.0, camera.focalDistance());
  }

  TEST(TiltShiftCamera, ShouldSetAndGetTilt) {
    TiltShiftCamera camera;
    camera.setTilt(20_degrees);
    EXPECT_DOUBLE_EQ(Angled(20_degrees).radians(), camera.tilt().radians());
  }

  TEST(TiltShiftCamera, ShouldSetAndGetShift) {
    TiltShiftCamera camera;
    camera.setShift(Vector2d(0.3, -0.2));
    EXPECT_DOUBLE_EQ(0.3, camera.shift().x());
    EXPECT_DOUBLE_EQ(-0.2, camera.shift().y());
  }

  TEST(TiltShiftCamera, ShouldDegenerateToThinLensWhenTiltAndShiftAreZero) {
    // The whole TiltShift contract is "tilt=0, shift=0 → identical to
    // ThinLens." Pin it so a future change to the focal-plane math
    // can't silently break the degenerate case.
    TiltShiftCamera ts(Vector3d(0, 0, -1), Vector3d::null);
    ts.setApertureRadius(0.5);
    ts.setFocalDistance(4);
    // tilt and shift left at default zero.

    ThinLensCamera ref(Vector3d(0, 0, -1), Vector3d::null);
    ref.setApertureRadius(0.5);
    ref.setFocalDistance(4);

    auto a = ts.rayForPixelWithLens(0, 0, 0.5, 0.5);
    auto b = ref.rayForPixelWithLens(0, 0, 0.5, 0.5);

    ASSERT_VECTOR_NEAR(a.origin(), b.origin(), 1e-12);
    ASSERT_VECTOR_NEAR(a.direction(), b.direction(), 1e-12);
  }

  TEST(TiltShiftCamera, ShouldStillConvergeRaysAtFocalPointUnderTilt) {
    // Even with a tilted focal plane, the load-bearing invariant of a
    // thin-lens camera holds: every lens sample for the same pixel
    // converges at the *same* focal point (which now lives on the
    // tilted plane instead of the perpendicular one). Without that,
    // depth-of-field for objects on the tilted plane breaks down.
    TiltShiftCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    camera.setApertureRadius(0.5);
    camera.setFocalDistance(4);
    camera.setTilt(20_degrees);

    // Each ray's focal point — extrapolate from origin along direction
    // far enough to reach the focal region, then check the trio of
    // points all collapse to one.
    auto pickPointFar = [&](double lu, double lv) {
      Rayd r = camera.rayForPixelWithLens(0.5, 0.5, lu, lv);
      // Extrapolate to a fixed t large enough to clear the lens disc;
      // at infinity all rays for the same pixel diverge by their
      // origin offset, so finite-t differences shrink toward zero
      // monotonically — but for a thin-lens camera they should be
      // *exactly* zero at the focal point. Pick a t that lands at the
      // focal point: solve for it from the principal ray.
      Rayd principal = camera.rayForPixelWithLens(0.5, 0.5, 0.0, 0.0);
      double t_focal = camera.focalDistance() + camera.distance();
      // For the principal (lensU=lensV=0) ray, the focal point is at
      // origin + direction * t_focal_along_dir. Compute that, then
      // for any other lens sample, the same focal point is reached
      // at t = (focal - origin) projected along direction.
      Vector3d focal = principal.origin() + principal.direction() * t_focal;
      double t = (Vector3d(focal) - Vector3d(r.origin())) * r.direction();
      return r.origin() + r.direction() * t;
    };

    Vector3d centre = pickPointFar(0.0, 0.0);
    Vector3d edgeR = pickPointFar(1.0, 0.0);
    Vector3d edgeU = pickPointFar(0.0, 1.0);

    ASSERT_VECTOR_NEAR(centre, edgeR, 1e-9);
    ASSERT_VECTOR_NEAR(centre, edgeU, 1e-9);
  }

  TEST(TiltShiftCamera, ShouldShiftOriginAlongLensDiscWithApertureRadius) {
    // Inherited DOF behaviour should still hold: with a non-zero
    // aperture, two different lens samples produce rays from
    // different origins.
    TiltShiftCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    camera.setApertureRadius(0.5);
    camera.setTilt(15_degrees);

    auto a = camera.rayForPixelWithLens(0, 0, 0.0, 0.0);
    auto b = camera.rayForPixelWithLens(0, 0, 1.0, 0.0);
    EXPECT_GT((a.origin() - b.origin()).length(), 0.1);
  }

  TEST(TiltShiftCamera, ProjectPointUsesInheritedPinholeProjectionFallback) {
    TiltShiftCamera camera(Vector3d(0.5, -0.25, -4.0), Vector3d(0.0, 0.0, 1.0));
    camera.setDistance(4.0);
    camera.setZoom(1.25);
    camera.setApertureRadius(0.7);
    camera.setFocalDistance(2.5);
    camera.setTilt(25_degrees);
    camera.setShift(Vector2d(0.3, -0.2));
    setupViewPlane(camera, 160, 90);

    PinholeCamera pinhole(camera.position(), camera.target());
    pinhole.setDistance(camera.distance());
    pinhole.setZoom(camera.zoom());
    setupViewPlane(pinhole, 160, 90);

    const Vector3d point(1.0, -0.5, 3.0);
    ASSERT_VECTOR_NEAR(pinhole.projectPoint(point), camera.projectPoint(point), 1e-9);
    ASSERT_VECTOR_NEAR(pinhole.projectPointWithDepth(point), camera.projectPointWithDepth(point),
                       1e-9);
    ASSERT_VECTOR_NEAR(pinhole.projectPointToClipSpace(point),
                       camera.projectPointToClipSpace(point), 1e-9);
    EXPECT_NEAR(pinhole.eyeRelativeDepth(point), camera.eyeRelativeDepth(point), 1e-9);
  }

  TEST(TiltShiftCamera, ShouldExposeStaticGpuPrimaryPathDescriptor) {
    TiltShiftCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d::null);
    camera.setApertureRadius(0.25);
    camera.setFocalDistance(6.0);
    camera.setTilt(20_degrees);
    camera.setShift(Vector2d(0.2, -0.1));
    setupViewPlane(camera, 3, 2);
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    const std::optional<GpuPrimaryPathDescriptor> descriptor =
      camera.gpuPrimaryPathDescriptor(Recti(0, 0, 3, 2), 1234);

    ASSERT_TRUE(descriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeTiltShift, descriptor->mode);
    EXPECT_TRUE(descriptor->generatesOnDevice());
    EXPECT_EQ(24u, descriptor->pathCount());

    const GpuRectilinearPrimaryPathDescriptor& rectilinear = descriptor->rectilinear;
    EXPECT_EQ(0, rectilinear.requestedLeft);
    EXPECT_EQ(0, rectilinear.requestedTop);
    EXPECT_EQ(3u, rectilinear.requestedWidth);
    EXPECT_EQ(2u, rectilinear.requestedHeight);
    EXPECT_EQ(0, rectilinear.actualLeft);
    EXPECT_EQ(0, rectilinear.actualTop);
    EXPECT_EQ(3u, rectilinear.actualWidth);
    EXPECT_EQ(2u, rectilinear.actualHeight);
    EXPECT_EQ(4u, rectilinear.samplesPerPixel);
    EXPECT_EQ(1234u, rectilinear.sampleSeed);
    EXPECT_FLOAT_EQ(11.0f, rectilinear.lensParameters[0]);
    EXPECT_FLOAT_EQ(0.2f, rectilinear.lensParameters[1]);
    EXPECT_FLOAT_EQ(-0.1f, rectilinear.lensParameters[2]);
    EXPECT_FLOAT_EQ(static_cast<float>((20_degrees).radians()), rectilinear.lensParameters[3]);
  }

  TEST(TiltShiftCamera, ShouldExposeFixedShutterAnimatedGpuPrimaryPathDescriptor) {
    TiltShiftCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    camera.setApertureRadius(0.25);
    camera.setFocalDistance(6.0);
    camera.setTilt(20_degrees);
    camera.setShift(Vector2d(0.2, -0.1));
    setupViewPlane(camera, 4, 3);
    camera.viewPlane()->sampler()->setup(1, 1, 17);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.25, 0.25);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));

    const auto descriptor = camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 3), 1234);

    ASSERT_TRUE(descriptor);
    EXPECT_EQ(gpuPrimaryPathGenerationModeTiltShift, descriptor->mode);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -9.5), descriptorOrigin(*descriptor), 1e-6);
    EXPECT_EQ(12u, descriptor->pathCount());
  }

  TEST(TiltShiftCamera, ShouldExposeSampledShutterRigTranslationGpuPrimaryPathDescriptor) {
    TiltShiftCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    setupViewPlane(camera, 4, 3);
    camera.viewPlane()->sampler()->setup(1, 1, 17);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(0.0, 0.0, 2.0)}}));

    const auto descriptor = camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 3), 1234);

    ASSERT_TRUE(descriptor);
    EXPECT_EQ(gpuPrimaryPathGenerationModeTiltShift, descriptor->mode);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -10.0), descriptorOrigin(*descriptor), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 2.0), descriptorMotionOriginDelta(*descriptor), 1e-6);
    EXPECT_EQ(12u, descriptor->pathCount());
  }

  TEST(TiltShiftCamera, ShouldExposeSampledShutterRotatingRigGpuPrimaryPathDescriptor) {
    TiltShiftCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    camera.setApertureRadius(0.25);
    camera.setFocalDistance(6.0);
    camera.setTilt(20_degrees);
    camera.setShift(Vector2d(0.2, -0.1));
    setupViewPlane(camera, 4, 3);
    camera.viewPlane()->sampler()->setup(1, 1, 17);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(1.0, 0.0, 2.0)}}));

    const auto descriptor = camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 3), 1234);

    ASSERT_TRUE(descriptor);
    EXPECT_EQ(gpuPrimaryPathGenerationModeTiltShift, descriptor->mode);
    EXPECT_EQ(gpuPrimaryPathMotionModeLookAt, descriptor->rectilinear.motionMode);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -5.0), descriptorOrigin(*descriptor), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 2.0), descriptorMotionOriginDelta(*descriptor), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d::null, descriptorMotionTarget(*descriptor), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(1.0, 0.0, 2.0), descriptorMotionTargetDelta(*descriptor), 1e-6);
    EXPECT_FLOAT_EQ(5.0f, descriptor->rectilinear.motionParameters[0]);
    EXPECT_FLOAT_EQ(0.25f, descriptor->rectilinear.motionParameters[1]);
    EXPECT_FLOAT_EQ(11.0f, descriptor->rectilinear.lensParameters[0]);
    EXPECT_FLOAT_EQ(0.2f, descriptor->rectilinear.lensParameters[1]);
    EXPECT_FLOAT_EQ(-0.1f, descriptor->rectilinear.lensParameters[2]);
    EXPECT_EQ(12u, descriptor->pathCount());
  }
}
