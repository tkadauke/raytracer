#include <gtest/gtest.h>

#include "render/cameras/PinholeCamera.h"
#include "render/cameras/ThinLensCamera.h"
#include "engine/raytracer/Raytracer.h"
#include "render/animation/AnimationTrack.h"
#include "render/primitives/Scene.h"
#include "render/viewplanes/PointInterlacedViewPlane.h"
#include "render/samplers/JitteredSampler.h"
#include "core/Buffer.h"

#include "test/helpers/CameraTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

#include <optional>

namespace ThinLensCameraTest {
  using namespace render;
  using namespace engine::raytracer;
  using test::setupViewPlane;

  Vector3d descriptorOrigin(const GpuPrimaryPathDescriptor& descriptor) {
    return Vector3d(descriptor.rectilinear.originOrDirection[0],
                    descriptor.rectilinear.originOrDirection[1],
                    descriptor.rectilinear.originOrDirection[2]);
  }

  TEST(ThinLensCamera, ShouldDefaultToCannedValues) {
    ThinLensCamera camera;
    EXPECT_DOUBLE_EQ(5.0, camera.distance());
    EXPECT_DOUBLE_EQ(1.0, camera.zoom());
    EXPECT_DOUBLE_EQ(0.1, camera.apertureRadius());
    EXPECT_DOUBLE_EQ(5.0, camera.focalDistance());
  }

  TEST(ThinLensCamera, ShouldSetAndGetDistance) {
    ThinLensCamera camera;
    camera.setDistance(7);
    EXPECT_DOUBLE_EQ(7.0, camera.distance());
  }

  TEST(ThinLensCamera, ShouldSetAndGetZoom) {
    ThinLensCamera camera;
    camera.setZoom(2);
    EXPECT_DOUBLE_EQ(2.0, camera.zoom());
  }

  TEST(ThinLensCamera, ShouldSetAndGetApertureRadius) {
    ThinLensCamera camera;
    camera.setApertureRadius(0.5);
    EXPECT_DOUBLE_EQ(0.5, camera.apertureRadius());
  }

  TEST(ThinLensCamera, ShouldClampNegativeApertureRadiusToZero) {
    ThinLensCamera camera;
    camera.setApertureRadius(-1.0);
    EXPECT_DOUBLE_EQ(0.0, camera.apertureRadius());
  }

  TEST(ThinLensCamera, ShouldSetAndGetFocalDistance) {
    ThinLensCamera camera;
    camera.setFocalDistance(3.5);
    EXPECT_DOUBLE_EQ(3.5, camera.focalDistance());
  }

  TEST(ThinLensCamera, ShouldRejectZeroOrNegativeFocalDistance) {
    // A focal distance of zero would put the focus plane at the lens
    // itself — every ray is the same one and the math degenerates. The
    // setter ignores non-positive inputs rather than clamping to a
    // tiny epsilon (which would silently shift the user's intended
    // focus). Pin so a future change to clamping is loud.
    ThinLensCamera camera;
    camera.setFocalDistance(0);
    EXPECT_DOUBLE_EQ(5.0, camera.focalDistance());
    camera.setFocalDistance(-2.5);
    EXPECT_DOUBLE_EQ(5.0, camera.focalDistance());
  }

  TEST(ThinLensCamera, ShouldDegenerateToPinholeWhenApertureIsZero) {
    // With apertureRadius = 0 the lens-disc sample is multiplied by
    // zero, so every (lensU, lensV) yields the same eyeOrigin. The
    // resulting ray must then be identical to the equivalent pinhole
    // ray at the same pixel.
    ThinLensCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    camera.setApertureRadius(0);

    Rayd a = camera.rayForPixelWithLens(0, 0, 0.5, 0.5);
    Rayd b = camera.rayForPixelWithLens(0, 0, -0.7, 0.2);
    ASSERT_VECTOR_NEAR(a.origin(), b.origin(), 1e-12);
    ASSERT_VECTOR_NEAR(a.direction(), b.direction(), 1e-12);
  }

  TEST(ThinLensCamera, ShouldShiftOriginAlongLensDiscWithApertureRadius) {
    // With a non-zero aperture, two different lens samples must produce
    // rays from different origins — that's what creates DOF blur for
    // out-of-focus geometry.
    ThinLensCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    camera.setApertureRadius(0.5);

    Rayd a = camera.rayForPixelWithLens(0, 0, 0.0, 0.0); // lens centre
    Rayd b = camera.rayForPixelWithLens(0, 0, 1.0, 0.0); // edge of disc
    EXPECT_GT((a.origin() - b.origin()).length(), 0.1);
  }

  TEST(ThinLensCamera, ShouldConvergeRaysAtFocalPlane) {
    // The contract that makes DOF physically correct: every ray for the
    // same pixel must pass through the same point on the focal plane,
    // regardless of where on the lens disc the ray starts. That's why
    // in-focus geometry stays sharp while out-of-focus geometry blurs.
    //
    // Test by sampling several lens positions for the same pixel,
    // intersecting each ray with the plane at z=focalDistance from the
    // eye, and verifying all hits land at the same point.
    ThinLensCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    camera.setApertureRadius(0.5);
    camera.setFocalDistance(4);

    auto hitFocalPlane = [&](double lu, double lv) {
      Rayd r = camera.rayForPixelWithLens(0, 0, lu, lv);
      // focalDistance is measured from the **camera position** (the
      // user-facing one set via setPosition), not the internal eye.
      // Camera at z=-1, focalDistance=4 → focal plane at z = -1 + 4 = 3.
      double targetZ = -1.0 + 4.0;
      double t = (targetZ - r.origin().z()) / r.direction().z();
      return r.origin() + r.direction() * t;
    };

    Vector3d centre = hitFocalPlane(0.0, 0.0);
    Vector3d edgeR = hitFocalPlane(1.0, 0.0);
    Vector3d edgeT = hitFocalPlane(0.0, 1.0);
    Vector3d edgeRT = hitFocalPlane(0.7, 0.7);

    ASSERT_VECTOR_NEAR(centre, edgeR, 1e-9);
    ASSERT_VECTOR_NEAR(centre, edgeT, 1e-9);
    ASSERT_VECTOR_NEAR(centre, edgeRT, 1e-9);
  }

  TEST(ThinLensCamera, ProjectPointUsesEquivalentPinholeProjection) {
    ThinLensCamera camera(Vector3d(0.5, -0.25, -4.0), Vector3d(0.0, 0.0, 1.0));
    camera.setDistance(4.0);
    camera.setZoom(1.25);
    camera.setApertureRadius(0.7);
    camera.setFocalDistance(2.5);
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

  TEST(ThinLensCamera, PrimaryRayGeneratorMatchesCallerOwnedStream) {
    ThinLensCamera camera(Vector3d(0.5, -0.25, -4.0), Vector3d(0.0, 0.0, 1.0));
    camera.setDistance(4.0);
    camera.setZoom(1.25);
    camera.setApertureRadius(0.7);
    camera.setFocalDistance(2.5);
    auto sampler = std::make_shared<JitteredSampler>();
    sampler->setup(4, 16);
    camera.viewPlane()->setSampler(sampler);
    setupViewPlane(camera, 160, 90);

    auto pixel = camera.viewPlane()->begin(Recti(0, 0, 160, 90));
    ++pixel;
    ++pixel;
    const auto tileSeed = std::optional<std::uint64_t>(1234);
    const std::uint64_t pixelHash = camera.primaryRayPixelHash(pixel, tileSeed);
    auto stream = sampler->stream(2, pixelHash);
    auto generatorStream = sampler->stream(2, pixelHash);

    const auto borrowedSample = camera.primaryRaySample(pixel, *stream);
    const auto generatedSample = camera.primaryRayGenerator()->sample(pixel, *generatorStream);

    ASSERT_TRUE(borrowedSample.has_value());
    ASSERT_TRUE(generatedSample.has_value());
    ASSERT_VECTOR_NEAR(borrowedSample->ray.origin(), generatedSample->ray.origin(), 1e-12);
    ASSERT_VECTOR_NEAR(borrowedSample->ray.direction(), generatedSample->ray.direction(), 1e-12);
    ASSERT_DOUBLE_EQ(borrowedSample->timeSample, generatedSample->timeSample);
  }

  TEST(ThinLensCamera, ShouldExposeFixedShutterAnimatedGpuPrimaryPathDescriptor) {
    ThinLensCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    camera.setApertureRadius(0.25);
    setupViewPlane(camera, 4, 3);
    camera.viewPlane()->sampler()->setup(1, 1, 17);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.25, 0.25);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));

    const auto descriptor = camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 3), 1234);

    ASSERT_TRUE(descriptor);
    EXPECT_EQ(gpuPrimaryPathGenerationModeThinLens, descriptor->mode);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -9.5), descriptorOrigin(*descriptor), 1e-6);
    EXPECT_EQ(12u, descriptor->pathCount());
  }

  TEST(ThinLensCamera, ShouldRejectSampledShutterAnimatedGpuPrimaryPathDescriptor) {
    ThinLensCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    setupViewPlane(camera, 4, 3);
    camera.viewPlane()->sampler()->setup(1, 1, 17);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));

    EXPECT_FALSE(camera.gpuPrimaryPathDescriptor(Recti(0, 0, 4, 3), 1234));
  }

  TEST(ThinLensCamera, ShouldRender) {
    // Smoke test — the random aperture sampler in the no-lens-arg
    // overload must not crash the render loop, and the result for an
    // empty white scene should be white. Set up the viewplane manually
    // (Camera::render doesn't, only Raytracer::render does) — the
    // ThinLens lens-disc math relies on the viewplane being sized.
    ThinLensCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 2, 2));
    auto scene = std::make_shared<Scene>(Colord::white());
    auto raytracer = std::make_shared<Raytracer>(scene);
    Buffer<Colord> buffer(2, 2);
    camera.render(raytracer, buffer);
    EXPECT_EQ(Colord::white(), buffer[0][0]);
  }

  TEST(ThinLensCamera, ShouldAutoInstallMultiSampleSamplerOnDefault1SppViewPlane) {
    // Default UI-preview flow: a fresh ViewPlane comes in with the
    // factory-default 1-spp render::RegularSampler. ThinLens should bump that
    // to a multi-sample sampler so the GUI render isn't confetti.
    ThinLensCamera camera;
    auto plane = std::make_shared<render::PointInterlacedViewPlane>();
    EXPECT_EQ(1, plane->sampler()->numSamples());

    camera.setViewPlane(plane);

    EXPECT_GT(plane->sampler()->numSamples(), 1)
      << "ThinLens should auto-install a multi-sample sampler when the "
         "incoming viewplane has the factory-default 1-spp sampler";
  }

  TEST(ThinLensCamera, ShouldRespectExistingMultiSampleSamplerOnIncomingViewPlane) {
    // Modeler's RenderWindow flow: caller attaches a chosen
    // multi-sample sampler to the viewplane BEFORE calling setViewPlane.
    // ThinLens must NOT clobber that sampler — doing so silently
    // discards the user's "Samples per pixel" UI setting.
    ThinLensCamera camera;
    auto plane = std::make_shared<render::PointInterlacedViewPlane>();
    auto userSampler = std::make_shared<render::JitteredSampler>();
    userSampler->setup(64, 7);
    plane->setSampler(userSampler);

    camera.setViewPlane(plane);

    EXPECT_EQ(userSampler.get(), plane->sampler().get())
      << "ThinLens must preserve a caller-supplied multi-sample sampler — "
         "a regression here makes the GUI render coarser than intended";
    EXPECT_EQ(64, plane->sampler()->numSamples());
  }
}
