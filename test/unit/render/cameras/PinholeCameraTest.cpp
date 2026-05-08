#include <gtest/gtest.h>
#include "render/cameras/PinholeCamera.h"
#include "engine/raytracer/Raytracer.h"
#include "render/primitives/Scene.h"
#include "render/tonemap/LinearTonemap.h"
#include "core/Buffer.h"

namespace PinholeCameraTest {
  using namespace ::testing;
  using namespace render;
  using namespace engine::raytracer;
using namespace render;
  using namespace engine::raytracer;
using namespace render;
  using namespace engine::raytracer;
  
  TEST(PinholeCamera, ShouldConstructWithoutParameters) {
    PinholeCamera camera;
    ASSERT_EQ(5, camera.distance());
    ASSERT_EQ(1, camera.zoom());
  }
  
  TEST(PinholeCamera, ShouldConstructWithParameters) {
    PinholeCamera camera(Vector3d(0, 0, 1), Vector3d::null());
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
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
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
    // covered by visual smoke-testing in GeneratedRayTracer.
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
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
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    auto scene = std::make_shared<Scene>(Colord::white());
    auto raytracer = std::make_shared<Raytracer>(scene);
    Buffer<Colord> buffer(1, 1);

    camera.setZoom(2);
    camera.render(raytracer, buffer);
    ASSERT_EQ(0.5, camera.viewPlane()->pixelSize());
  }
  
  TEST(PinholeCamera, ShouldGetRayForPixelWithUninitializedViewPlane) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Rayd ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -6), ray.origin());
    ASSERT_EQ(Vector3d(0, 0, 1), ray.direction());
  }
  
  TEST(PinholeCamera, ShouldGetRayForPixelWithInitializedViewPlane) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    auto raytracer = std::make_shared<Raytracer>(std::make_shared<Scene>(Colord::white()));
    Buffer<Colord> buffer(1, 1);
    camera.render(raytracer, buffer);

    Rayd ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -6), ray.origin());
    ASSERT_EQ(Vector3d(0, 0, 1), ray.direction());
  }

  // Helper: install a 100×100 view plane on the camera so projectPoint
  // has a non-degenerate window to map into. Without this, viewPlane
  // width/height are zero and the math degenerates.
  static void initViewPlane(PinholeCamera& camera, int width = 100, int height = 100) {
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, width, height));
  }

  static Vector3d screenFromClip(const Vector4d& clip, int width, int height) {
    const double invW = 1.0 / clip.w();
    return Vector3d(
      (clip.x() * invW + 1.0) * width / 2.0,
      (clip.y() * invW + 1.0) * height / 2.0,
      clip.z()
    );
  }

  TEST(PinholeCamera, ProjectsCameraTargetToImageCenter) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    initViewPlane(camera);

    // The camera looks at the origin. Origin should project to the
    // pixel-grid centre of a 100×100 window.
    Vector2d projected = camera.projectPoint(Vector3d::null());
    EXPECT_NEAR(50.0, projected.x(), 1e-9);
    EXPECT_NEAR(50.0, projected.y(), 1e-9);
  }

  TEST(PinholeCamera, ProjectsPointAtEyeToUndefined) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    initViewPlane(camera);
    // Eye is at world (0, 0, -6) — z=-1 camera position pulled back
    // by distance=5.
    Vector2d projected = camera.projectPoint(Vector3d(0, 0, -6));
    EXPECT_TRUE(projected.isUndefined());
  }

  TEST(PinholeCamera, ProjectsPointBehindEyeToUndefined) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    initViewPlane(camera);
    // Anything further from origin than the eye on the same ray
    // points the wrong way through the pinhole.
    Vector2d projected = camera.projectPoint(Vector3d(0, 0, -10));
    EXPECT_TRUE(projected.isUndefined());
  }

  TEST(PinholeCamera, RoundTripsThroughRayForPixel) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    initViewPlane(camera, 200, 150);

    // For each of a handful of pixels, generate the primary ray, walk
    // along it to a point well in front of the eye, and project that
    // point back to a pixel. Should match the original pixel modulo
    // floating-point drift.
    const std::pair<double, double> samples[] = {
      {50.0, 50.0}, {100.0, 75.0}, {25.0, 120.0}, {175.0, 30.0}
    };
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
    PinholeCamera camera(Vector3d(0, -3, -10), Vector3d::null());
    camera.setZoom(1.5);
    initViewPlane(camera, 320, 240);

    const std::pair<double, double> samples[] = {
      {160.0, 120.0}, {64.0, 60.0}, {256.0, 180.0}, {80.0, 200.0}
    };
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
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    camera.setZoom(2.0);
    initViewPlane(camera);

    // A point off-axis at world (1, 0, 5) projects somewhere right
    // of centre in zoom-1, further right in zoom-2 (because the
    // angular field of view shrinks). Verify monotonicity.
    Vector2d zoom2 = camera.projectPoint(Vector3d(1, 0, 5));

    PinholeCamera unzoomed(Vector3d(0, 0, -1), Vector3d::null());
    initViewPlane(unzoomed);
    Vector2d zoom1 = unzoomed.projectPoint(Vector3d(1, 0, 5));

    EXPECT_GT(zoom2.x(), zoom1.x());
  }

  TEST(PinholeCamera, ClipSpaceProjectionMatchesProjectionWithDepth) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    initViewPlane(camera, 200, 150);

    const Vector3d point(1.5, -0.5, 4.0);
    const Vector4d clip = camera.projectPointToClipSpace(point);
    const Vector3d projected = camera.projectPointWithDepth(point);
    const Vector3d fromClip = screenFromClip(clip, 200, 150);

    ASSERT_FALSE(clip.isUndefined());
    EXPECT_NEAR(projected.x(), fromClip.x(), 1e-9);
    EXPECT_NEAR(projected.y(), fromClip.y(), 1e-9);
    EXPECT_NEAR(projected.z(), fromClip.z(), 1e-9);
  }

  TEST(PinholeCamera, ClipSpaceProjectionKeepsBehindEyePointsRepresentable) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    initViewPlane(camera);

    const Vector4d atEye = camera.projectPointToClipSpace(Vector3d(0, 0, -6));
    const Vector4d behindEye = camera.projectPointToClipSpace(Vector3d(0, 0, -10));

    EXPECT_FALSE(atEye.isUndefined());
    EXPECT_FALSE(behindEye.isUndefined());
    EXPECT_EQ(0.0, atEye.w());
    EXPECT_LT(behindEye.w(), 0.0);
  }
}
