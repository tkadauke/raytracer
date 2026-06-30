#include <gtest/gtest.h>
#include "render/cameras/PinholeCamera.h"
#include "engine/raytracer/Raytracer.h"
#include "render/animation/AnimationTrack.h"
#include "render/primitives/Scene.h"
#include "render/samplers/Sampler.h"
#include "render/tonemap/LinearTonemap.h"
#include "core/Buffer.h"

#include "test/helpers/CameraTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

namespace PinholeCameraTest {
  using namespace ::testing;
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

  TEST(PinholeCamera, ShouldConstructWithoutParameters) {
    PinholeCamera camera;
    ASSERT_EQ(5, camera.distance());
    ASSERT_EQ(1, camera.zoom());
  }

  TEST(PinholeCamera, ShouldConstructWithParameters) {
    PinholeCamera camera(Vector3d(0, 0, 1), Vector3d::null);
    ASSERT_EQ(5, camera.distance());
    ASSERT_EQ(1, camera.zoom());
  }

  TEST(PinholeCamera, ShouldSetDistance) {
    PinholeCamera camera;
    camera.setDistance(20);
    ASSERT_EQ(20, camera.distance());
  }

  TEST(PinholeCamera, ShouldSetZoom) {
    PinholeCamera camera;
    camera.setZoom(2);
    ASSERT_EQ(2, camera.zoom());
  }

  TEST(PinholeCamera, ShouldRender) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    auto scene = std::make_shared<Scene>(Colord::white());
    auto raytracer = std::make_shared<Raytracer>(scene);
    Buffer<Colord> buffer(1, 1);
    camera.render(raytracer, buffer);
    ASSERT_EQ(Colord::white(), buffer[0][0]);
  }

  TEST(PinholeCamera, RendersIntoLdrBufferWithInlineTonemap) {
    // The LDR camera-render path is what makes progressive display
    // work in the GUI: each pixel is tonemapped + packed to RGB by
    // the worker thread as it goes, so a polling display widget
    // sees output before the render finishes. This test verifies
    // the path produces correct pixel values; the
    // mid-render-non-empty property is timing-dependent and
    // covered by visual smoke-testing in Modeler.
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    auto scene = std::make_shared<Scene>(Colord::white());
    auto raytracer = std::make_shared<Raytracer>(scene);
    Buffer<unsigned int> buffer(1, 1);
    auto tonemap = std::make_shared<render::LinearTonemap>();

    // The view plane needs setup the same way Raytracer::render
    // does; bypass the dispatch wrapper and call the tile render
    // directly.
    camera.viewPlane()->setup(camera.matrix(), buffer.rect());
    camera.render(raytracer, buffer, tonemap, buffer.rect());

    // Linear tonemap of `Colord::white()` is `0xffffffff` (full
    // alpha + RGB). The `rgb()` packing produces this fixed value.
    EXPECT_EQ(Colord::white().rgb(), buffer[0][0]);
  }

  TEST(PinholeCamera, ShouldSetViewplanePixelSize) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    auto scene = std::make_shared<Scene>(Colord::white());
    auto raytracer = std::make_shared<Raytracer>(scene);
    Buffer<Colord> buffer(1, 1);

    camera.setZoom(2);
    camera.render(raytracer, buffer);
    ASSERT_EQ(0.5, camera.viewPlane()->pixelSize());
  }

  TEST(PinholeCamera, ShouldGetRayForPixelWithUninitializedViewPlane) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    Rayd ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -6), ray.origin());
    ASSERT_EQ(Vector3d(0, 0, 1), ray.direction());
  }

  TEST(PinholeCamera, ShouldGetRayForPixelWithInitializedViewPlane) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    auto raytracer = std::make_shared<Raytracer>(std::make_shared<Scene>(Colord::white()));
    Buffer<Colord> buffer(1, 1);
    camera.render(raytracer, buffer);

    Rayd ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -6), ray.origin());
    ASSERT_EQ(Vector3d(0, 0, 1), ray.direction());
  }

  TEST(PinholeCamera, ShouldExposeFixedShutterAnimatedGpuPrimaryPathDescriptor) {
    PinholeCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    setupViewPlane(camera, 4, 3);
    camera.viewPlane()->sampler()->setup(1, 1, 17);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.25, 0.25);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));

    const auto descriptor = camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 3), 1234);

    ASSERT_TRUE(descriptor);
    EXPECT_EQ(gpuPrimaryPathGenerationModePinhole, descriptor->mode);
    EXPECT_EQ(gpuPrimaryPathMotionModeOriginDelta, descriptor->rectilinear.motionMode);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -9.5), descriptorOrigin(*descriptor), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d::null, descriptorMotionOriginDelta(*descriptor), 1e-6);
    EXPECT_EQ(12u, descriptor->pathCount());
  }

  TEST(PinholeCamera, ShouldExposeSampledShutterRigTranslationGpuPrimaryPathDescriptor) {
    PinholeCamera camera(Vector3d(0, 0, -5), Vector3d::null);
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
    EXPECT_EQ(gpuPrimaryPathGenerationModePinhole, descriptor->mode);
    EXPECT_EQ(gpuPrimaryPathMotionModeOriginDelta, descriptor->rectilinear.motionMode);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -10.0), descriptorOrigin(*descriptor), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 2.0), descriptorMotionOriginDelta(*descriptor), 1e-6);
    EXPECT_EQ(12u, descriptor->pathCount());
  }

  TEST(PinholeCamera, ShouldExposeSampledShutterPositionOnlyGpuPrimaryPathDescriptor) {
    PinholeCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    setupViewPlane(camera, 4, 3);
    camera.viewPlane()->sampler()->setup(1, 1, 17);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));

    const auto descriptor = camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 3), 1234);

    ASSERT_TRUE(descriptor);
    EXPECT_EQ(gpuPrimaryPathGenerationModePinhole, descriptor->mode);
    EXPECT_EQ(gpuPrimaryPathMotionModeLookAt, descriptor->rectilinear.motionMode);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -5.0), descriptorOrigin(*descriptor), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 2.0), descriptorMotionOriginDelta(*descriptor), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 0.0), descriptorMotionTarget(*descriptor), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d::null, descriptorMotionTargetDelta(*descriptor), 1e-6);
    EXPECT_EQ(5.0f, descriptor->rectilinear.motionParameters[0]);
    EXPECT_EQ(12u, descriptor->pathCount());
  }

  TEST(PinholeCamera, ShouldExposeSampledShutterRotatingRigGpuPrimaryPathDescriptor) {
    PinholeCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    setupViewPlane(camera, 4, 3);
    camera.viewPlane()->sampler()->setup(1, 1, 17);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(1.0, 0.0, 2.0)}}));

    const auto descriptor = camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 3), 1234);

    ASSERT_TRUE(descriptor);
    EXPECT_EQ(gpuPrimaryPathGenerationModePinhole, descriptor->mode);
    EXPECT_EQ(gpuPrimaryPathMotionModeLookAt, descriptor->rectilinear.motionMode);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -5.0), descriptorOrigin(*descriptor), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 2.0), descriptorMotionOriginDelta(*descriptor), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 0.0), descriptorMotionTarget(*descriptor), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(1.0, 0.0, 2.0), descriptorMotionTargetDelta(*descriptor), 1e-6);
    EXPECT_EQ(5.0f, descriptor->rectilinear.motionParameters[0]);
    EXPECT_EQ(12u, descriptor->pathCount());
  }

  TEST(PinholeCamera, ShouldRejectSampledShutterPinholeDescriptorWhenShutterCrossesKeyframe) {
    PinholeCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    setupViewPlane(camera, 4, 3);
    camera.viewPlane()->sampler()->setup(1, 1, 17);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack({{0.0, Vector3d(0.0, 0.0, -5.0)},
                                                                {0.5, Vector3d(0.0, 0.0, -4.0)},
                                                                {1.0, Vector3d(0.0, 0.0, -3.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack({{0.0, Vector3d(0.0, 0.0, 0.0)},
                                                                {0.5, Vector3d(0.0, 0.0, 1.0)},
                                                                {1.0, Vector3d(0.0, 0.0, 2.0)}}));

    EXPECT_FALSE(camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 3), 1234));
  }

  TEST(PinholeCamera, ShouldRejectSampledShutterPinholeDescriptorWhenDirectionCollapses) {
    PinholeCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    setupViewPlane(camera, 4, 3);
    camera.viewPlane()->sampler()->setup(1, 1, 17);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, 5.0)}}));

    EXPECT_FALSE(camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 3), 1234));
  }

  TEST(PinholeCamera, ProjectsCameraTargetToImageCenter) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    setupViewPlane(camera);

    // The camera looks at the origin. Origin should project to the
    // pixel-grid centre of a 100×100 window.
    Vector2d projected = camera.projectPoint(Vector3d::null);
    EXPECT_NEAR(50.0, projected.x(), 1e-9);
    EXPECT_NEAR(50.0, projected.y(), 1e-9);
  }

  TEST(PinholeCamera, ProjectsPointAtEyeToUndefined) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    setupViewPlane(camera);
    // Eye is at world (0, 0, -6) — z=-1 camera position pulled back
    // by distance=5.
    Vector2d projected = camera.projectPoint(Vector3d(0, 0, -6));
    EXPECT_TRUE(projected.isUndefined());
  }

  TEST(PinholeCamera, ProjectsPointBehindEyeToUndefined) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    setupViewPlane(camera);
    // Anything further from origin than the eye on the same ray
    // points the wrong way through the pinhole.
    Vector2d projected = camera.projectPoint(Vector3d(0, 0, -10));
    EXPECT_TRUE(projected.isUndefined());
  }

  TEST(PinholeCamera, RoundTripsThroughRayForPixel) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    setupViewPlane(camera, 200, 150);

    // For each of a handful of pixels, generate the primary ray, walk
    // along it to a point well in front of the eye, and project that
    // point back to a pixel. Should match the original pixel modulo
    // floating-point drift.
    const std::pair<double, double> samples[] = {
      {50.0, 50.0}, {100.0, 75.0}, {25.0, 120.0}, {175.0, 30.0}};
    for (const auto& [x, y] : samples) {
      Rayd ray = camera.rayForPixel(x, y);
      Vector3d worldPoint = ray.at(20.0);
      Vector2d back = camera.projectPoint(worldPoint);
      ASSERT_FALSE(back.isUndefined()) << "for (" << x << ", " << y << ")";
      EXPECT_NEAR(x, back.x(), 1e-7) << "x for (" << x << ", " << y << ")";
      EXPECT_NEAR(y, back.y(), 1e-7) << "y for (" << x << ", " << y << ")";
    }
  }

  TEST(PinholeCamera, RoundTripsWithNonUnitZoomAndOffAxisCamera) {
    // Regression test for the wireframe-vs-raytracer alignment bug.
    // pixelAt() scales the world-space pixel by pixelSize, which moves
    // the view plane along with the camera position; projectPoint has
    // to account for the resulting (pixelSize - 1) * (R^-1 * E) offset.
    // A unit-zoom round trip masks the bug; this test uses zoom=1.5
    // and a camera positioned off-axis (matching the glass-torus demo
    // scene) so any miscompensation surfaces.
    PinholeCamera camera(Vector3d(0, -3, -10), Vector3d::null);
    camera.setZoom(1.5);
    setupViewPlane(camera, 320, 240);

    const std::pair<double, double> samples[] = {
      {160.0, 120.0}, {64.0, 60.0}, {256.0, 180.0}, {80.0, 200.0}};
    for (const auto& [x, y] : samples) {
      Rayd ray = camera.rayForPixel(x, y);
      Vector3d worldPoint = ray.at(15.0);
      Vector2d back = camera.projectPoint(worldPoint);
      ASSERT_FALSE(back.isUndefined()) << "for (" << x << ", " << y << ")";
      EXPECT_NEAR(x, back.x(), 1e-6) << "x for (" << x << ", " << y << ")";
      EXPECT_NEAR(y, back.y(), 1e-6) << "y for (" << x << ", " << y << ")";
    }
  }

  TEST(PinholeCamera, ProjectionRespectsZoom) {
    // At zoom=2, the same world point should project to a pixel
    // closer to the image centre because the view plane is "smaller"
    // (each pixel covers less world space).
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    camera.setZoom(2.0);
    setupViewPlane(camera);

    // A point off-axis at world (1, 0, 5) projects somewhere right
    // of centre in zoom-1, further right in zoom-2 (because the
    // angular field of view shrinks). Verify monotonicity.
    Vector2d zoom2 = camera.projectPoint(Vector3d(1, 0, 5));

    PinholeCamera unzoomed(Vector3d(0, 0, -1), Vector3d::null);
    setupViewPlane(unzoomed);
    Vector2d zoom1 = unzoomed.projectPoint(Vector3d(1, 0, 5));

    EXPECT_GT(zoom2.x(), zoom1.x());
  }

  TEST(PinholeCamera, ClipSpaceProjectionMatchesProjectionWithDepth) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
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

  TEST(PinholeCamera, WorldToClipMatrixMatchesPerVertexProjection) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d(0, 0.2, 1));
    setupViewPlane(camera, 320, 240);

    const auto matrix = camera.worldToClipMatrix();
    ASSERT_TRUE(matrix.has_value());

    // Sampled world points covering the view frustum's interior.
    const Vector3d points[] = {
      Vector3d(0, 0, 0), Vector3d(1, 0, 0),  Vector3d(0, 1, 0),
      Vector3d(0, 0, 1), Vector3d(-2, 3, 4), Vector3d(0.5, -0.25, 2.5),
    };
    for (const Vector3d& world : points) {
      const Vector4d expected = camera.projectPointToClipSpace(world);
      const Vector4d through = *matrix * Vector4d(world.x(), world.y(), world.z(), 1.0);
      // The per-vertex form stashes eye-depth in z/w; the matrix form gives
      // the standard OpenGL clip-space z that the perspective divide
      // yields NDC z in [-1, 1] for. Compare on x/y (post-divide) and w
      // (eye-depth equivalence). The matrix flips Y so positive world Y
      // lands at the bottom of the GL framebuffer, matching the project's
      // Y-down screen convention.
      EXPECT_NEAR(expected.x(), through.x(), 1e-9);
      EXPECT_NEAR(-expected.y(), through.y(), 1e-9);
      EXPECT_NEAR(expected.w(), through.w(), 1e-9);
    }
  }

  TEST(PinholeCamera, ClipSpaceProjectionKeepsBehindEyePointsRepresentable) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    setupViewPlane(camera);

    const Vector4d atEye = camera.projectPointToClipSpace(Vector3d(0, 0, -6));
    const Vector4d behindEye = camera.projectPointToClipSpace(Vector3d(0, 0, -10));

    EXPECT_FALSE(atEye.isUndefined());
    EXPECT_FALSE(behindEye.isUndefined());
    EXPECT_EQ(0.0, atEye.w());
    EXPECT_LT(behindEye.w(), 0.0);
  }

  TEST(PinholeCamera, ProjectionMatrixMapsViewPlaneEdgeToNdcOne) {
    // projectionMatrix() is built via Matrix4::frustum; verify the canonical
    // property: a point at the right edge of the view plane (x = halfW) at
    // the near plane (z = distance) should project to NDC x = +1.
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    setupViewPlane(camera, 200, 150);

    auto plane = camera.viewPlane();
    const double halfW = plane->hSpan() * plane->pixelSize() / 2.0;
    const double near = camera.distance();

    const Matrix4d m = camera.projectionMatrix();
    const Vector4d v = m * Vector4d(halfW, 0, near, 1.0);
    EXPECT_NEAR(1.0, v.x() / v.w(), 1e-9);
  }

  TEST(PinholeCamera, ProjectionMatrixMapsViewPlaneTopEdgeToNdcOne) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    setupViewPlane(camera, 200, 150);

    auto plane = camera.viewPlane();
    const double halfH = plane->vSpan() * plane->pixelSize() / 2.0;
    const double near = camera.distance();

    const Matrix4d m = camera.projectionMatrix();
    const Vector4d v = m * Vector4d(0, halfH, near, 1.0);
    EXPECT_NEAR(1.0, v.y() / v.w(), 1e-9);
  }

  TEST(PinholeCamera, ProjectionMatrixIsConsistentWithProjectPointToClipSpace) {
    // The x/w and y/w components of projectPointToClipSpace must match those
    // produced by applying projectionMatrix() to the eye-shifted camera point.
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    setupViewPlane(camera, 200, 150);

    const Vector3d worldPoint(2.0, -1.0, 3.0);
    const Vector4d clip = camera.projectPointToClipSpace(worldPoint);

    const Vector3d pCam = camera.inverseMatrix() * Vector4d(worldPoint);
    const double depth = pCam.z() + camera.distance();
    const Vector4d v = camera.projectionMatrix() * Vector4d(pCam.x(), pCam.y(), depth, 1.0);

    ASSERT_FALSE(clip.isUndefined());
    EXPECT_NEAR(v.x() / v.w(), clip.x() / clip.w(), 1e-9);
    EXPECT_NEAR(v.y() / v.w(), clip.y() / clip.w(), 1e-9);
  }
}
