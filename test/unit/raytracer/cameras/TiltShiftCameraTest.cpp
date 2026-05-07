#include <gtest/gtest.h>

#include "raytracer/cameras/TiltShiftCamera.h"
#include "raytracer/Raytracer.h"
#include "raytracer/primitives/Scene.h"

#include "test/helpers/VectorTestHelper.h"

namespace TiltShiftCameraTest {
  using namespace raytracer;
using namespace render;

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
    EXPECT_DOUBLE_EQ( 0.3, camera.shift().x());
    EXPECT_DOUBLE_EQ(-0.2, camera.shift().y());
  }

  TEST(TiltShiftCamera, ShouldDegenerateToThinLensWhenTiltAndShiftAreZero) {
    // The whole TiltShift contract is "tilt=0, shift=0 → identical to
    // ThinLens." Pin it so a future change to the focal-plane math
    // can't silently break the degenerate case.
    TiltShiftCamera ts(Vector3d(0, 0, -1), Vector3d::null());
    ts.setApertureRadius(0.5);
    ts.setFocalDistance(4);
    // tilt and shift left at default zero.

    ThinLensCamera ref(Vector3d(0, 0, -1), Vector3d::null());
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
    TiltShiftCamera camera(Vector3d(0, 0, -1), Vector3d::null());
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
    Vector3d edgeR  = pickPointFar(1.0, 0.0);
    Vector3d edgeU  = pickPointFar(0.0, 1.0);

    ASSERT_VECTOR_NEAR(centre, edgeR, 1e-9);
    ASSERT_VECTOR_NEAR(centre, edgeU, 1e-9);
  }

  TEST(TiltShiftCamera, ShouldShiftOriginAlongLensDiscWithApertureRadius) {
    // Inherited DOF behaviour should still hold: with a non-zero
    // aperture, two different lens samples produce rays from
    // different origins.
    TiltShiftCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    camera.setApertureRadius(0.5);
    camera.setTilt(15_degrees);

    auto a = camera.rayForPixelWithLens(0, 0, 0.0, 0.0);
    auto b = camera.rayForPixelWithLens(0, 0, 1.0, 0.0);
    EXPECT_GT((a.origin() - b.origin()).length(), 0.1);
  }
}
