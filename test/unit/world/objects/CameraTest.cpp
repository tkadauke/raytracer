#include <gtest/gtest.h>

#include "world/objects/Camera.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/OrthographicCamera.h"
#include "world/objects/SphericalCamera.h"
#include "world/objects/FishEyeCamera.h"

#include "render/cameras/Camera.h"
#include "render/cameras/PinholeCamera.h"
#include "render/cameras/OrthographicCamera.h"
#include "render/cameras/SphericalCamera.h"
#include "render/cameras/FishEyeCamera.h"

#include "core/math/Angle.h"

namespace CameraTest {
  // ---------- Camera (abstract base) ----------------------------------------

  TEST(Camera, ShouldDefaultPositionToOneUnitInFrontOfOrigin) {
    // Default position (0, 0, -1) and target (0, 0, 0) — i.e. the camera
    // sits at z=-1 looking at the origin. Concrete cameras (pinhole,
    // orthographic, ...) inherit these.
    PinholeCamera camera;
    EXPECT_EQ(Vector3d(0, 0, -1), camera.position());
  }

  TEST(Camera, ShouldDefaultTargetToOrigin) {
    PinholeCamera camera;
    EXPECT_EQ(Vector3d(0, 0, 0), camera.target());
  }

  TEST(Camera, ShouldSetAndGetPositionAndTarget) {
    PinholeCamera camera;
    camera.setPosition(Vector3d(1, 2, 3));
    camera.setTarget(Vector3d(4, 5, 6));
    EXPECT_EQ(Vector3d(1, 2, 3), camera.position());
    EXPECT_EQ(Vector3d(4, 5, 6), camera.target());
  }

  // ---------- PinholeCamera -------------------------------------------------

  TEST(PinholeCamera, ShouldDefaultToCannedDistanceAndZoom) {
    PinholeCamera camera;
    EXPECT_DOUBLE_EQ(5.0, camera.distance());
    EXPECT_DOUBLE_EQ(1.0, camera.zoom());
  }

  TEST(PinholeCamera, ShouldClampZeroOrNegativeZoomToOne) {
    // setZoom replaces ≤0 with 1 instead of clamping at a tiny epsilon —
    // the camera math (1/zoom factor in viewplane mapping) is undefined
    // at zero, so the setter falls back to the canned default.
    PinholeCamera camera;
    camera.setZoom(0);
    EXPECT_DOUBLE_EQ(1.0, camera.zoom());
    camera.setZoom(-3);
    EXPECT_DOUBLE_EQ(1.0, camera.zoom());
  }

  TEST(PinholeCamera, ShouldSetAndGetPositiveZoom) {
    PinholeCamera camera;
    camera.setZoom(2.5);
    EXPECT_DOUBLE_EQ(2.5, camera.zoom());
  }

  TEST(PinholeCamera, ShouldProduceRaytracerPinholeCamera) {
    PinholeCamera camera;
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<render::PinholeCamera>(camera.toRaytracer()));
  }

  // ---------- OrthographicCamera --------------------------------------------

  TEST(OrthographicCamera, ShouldDefaultToZoomOne) {
    OrthographicCamera camera;
    EXPECT_DOUBLE_EQ(1.0, camera.zoom());
  }

  TEST(OrthographicCamera, ShouldClampZeroOrNegativeZoomToOne) {
    OrthographicCamera camera;
    camera.setZoom(-1);
    EXPECT_DOUBLE_EQ(1.0, camera.zoom());
  }

  TEST(OrthographicCamera, ShouldProduceRaytracerOrthographicCamera) {
    OrthographicCamera camera;
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<render::OrthographicCamera>(camera.toRaytracer()));
  }

  // ---------- SphericalCamera -----------------------------------------------

  TEST(SphericalCamera, ShouldDefaultToWideHorizontalFieldOfView) {
    SphericalCamera camera;
    EXPECT_DOUBLE_EQ((180_degrees).radians(), camera.horizontalFieldOfView().radians());
  }

  TEST(SphericalCamera, ShouldDefaultToNarrowerVerticalFieldOfView) {
    // Default 120° vertical vs 180° horizontal — the canned aspect-aware
    // settings used by every existing example. Pin both so an "isotropic"
    // change is deliberate.
    SphericalCamera camera;
    EXPECT_DOUBLE_EQ((120_degrees).radians(), camera.verticalFieldOfView().radians());
  }

  TEST(SphericalCamera, ShouldSetAndGetFieldOfView) {
    SphericalCamera camera;
    camera.setHorizontalFieldOfView(90_degrees);
    camera.setVerticalFieldOfView(60_degrees);
    EXPECT_DOUBLE_EQ((90_degrees).radians(), camera.horizontalFieldOfView().radians());
    EXPECT_DOUBLE_EQ((60_degrees).radians(), camera.verticalFieldOfView().radians());
  }

  TEST(SphericalCamera, ShouldProduceRaytracerSphericalCamera) {
    SphericalCamera camera;
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<render::SphericalCamera>(camera.toRaytracer()));
  }

  // ---------- FishEyeCamera -------------------------------------------------

  TEST(FishEyeCamera, ShouldDefaultToFullHemisphereFieldOfView) {
    FishEyeCamera camera;
    EXPECT_DOUBLE_EQ((180_degrees).radians(), camera.fieldOfView().radians());
  }

  TEST(FishEyeCamera, ShouldSetAndGetFieldOfView) {
    FishEyeCamera camera;
    camera.setFieldOfView(270_degrees);
    EXPECT_DOUBLE_EQ((270_degrees).radians(), camera.fieldOfView().radians());
  }

  TEST(FishEyeCamera, ShouldProduceRaytracerFishEyeCamera) {
    FishEyeCamera camera;
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<render::FishEyeCamera>(camera.toRaytracer()));
  }
}
