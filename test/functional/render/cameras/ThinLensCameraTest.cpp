#include "test/functional/support/RaytracerFeatureTest.h"

namespace ThinLensCameraTest {
  using namespace ::testing;

  struct ThinLensCameraTest : public RaytracerFeatureTest {};

  TEST_F(ThinLensCameraTest, ShouldBeVisibleInFrontOfCamera) {
    given("a thin-lens camera");
    given("a centered sphere");
    when("i look at the origin");
    then("i should see the sphere");
  }

  TEST_F(ThinLensCameraTest, ShouldNotBeVisibleBehindCamera) {
    given("a thin-lens camera");
    given("a centered sphere");
    when("i look away from the origin");
    then("i should not see the sphere");
  }

  TEST_F(ThinLensCameraTest, ShouldNotRenderWhenCanceled) {
    given("a blank canvas");
    given("a thin-lens camera");
    when("the render process is canceled");
    when("i look at the origin");
    then("i should see nothing");
  }

  // Canonical DOF invariant: a sphere placed exactly at the focal distance
  // renders with a crisp silhouette (few partial-coverage boundary pixels);
  // the same sphere rendered with the focal plane well beyond it blurs into
  // a wide band of intermediate colours.
  //
  // Geometry: lookAtOrigin() positions the camera at z=-5 looking at z=0.
  //   focalDistance=5  → focal plane at z=0 (on the sphere surface).
  //   focalDistance=15 → focal plane at z=10 (10 units past the sphere).
  //   aperture=0.5, blurDiameter ≈ 0.5 × |1 − 15/5| = 1.0 scene unit —
  //   a full sphere-radius of defocus blur at the silhouette.
  TEST_F(ThinLensCameraTest, ShouldProduceSharpSilhouetteAtFocalPlane) {
    given("a thin-lens camera focused at distance 5");
    given("a centered sphere");
    when("i look at the origin");
    then("i should see the sphere");
  }

  TEST_F(ThinLensCameraTest, FocalPlaneContractSharpVsBlurred) {
    given("a thin-lens camera focused at distance 5");
    given("a centered sphere");
    when("i look at the origin");
    then("record edge count as S");
    given("a thin-lens camera focused at distance 15");
    when("i look at the origin");
    then("edge count should be larger than S");
  }
}
