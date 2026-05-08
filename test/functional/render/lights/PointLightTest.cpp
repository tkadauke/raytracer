#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "core/Color.h"
#include "render/cameras/Camera.h"
#include "render/lights/PointLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Plane.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"

#include <cmath>
#include <string>

using namespace testing;
using namespace render;

// Scene geometry for shadow-boundary tests:
//
//   Light:   (0, 5, 0)  - white point light above the scene
//   Sphere:  centre (0, 1, 0), radius 1  - opaque occluder
//   Floor:   Plane(normal=(0,1,0), d=3) -> equation y = -3
//   Camera:  (0,0,-5) -> (0,0,0)  (lookAtOrigin)
//
// Shadow-centre on the floor:
//   Line from L=(0,5,0) through C=(0,1,0) -> direction (0,-1,0)
//   Hits y=-3 at t=8: world point (0,-3,0) -> pixel ~(100,38)
//
// Umbra boundary (2-D tangent from light to sphere, then to floor):
//   |LC|=4, r=1  -> sin theta=1/4, boundary x ~= +/-2.07 at y=-3
//
// Boundary radius at the floor:
//   8 * tan(asin(1/4)) = 8 / sqrt(15) ~= 2.07
//   x=1.8 is still shadowed; x=2.4 is lit.

namespace PointLightTest {

  struct PointLightTest : public RaytracerFeatureTest {};

  GIVEN(EngineFeatureTest, "a point light at \\(([\\-\\d.]+), ([\\-\\d.]+), ([\\-\\d.]+)\\)") {
    test->scene()->addLight(std::make_shared<PointLight>(
      Vector3d(std::stod(match[1]), std::stod(match[2]), std::stod(match[3])), Colord::white()));
  }

  // Sphere needs no material: shadow rays only need geometry; camera rays
  // that do hit the sphere return black (harmless for the floor pixels tested).
  GIVEN(EngineFeatureTest, "a unit sphere occluder at \\(0, 1, 0\\)") {
    test->add(std::make_shared<Sphere>(Vector3d(0, 1, 0), 1.0));
  }

  // Plane equation: n.p + d = 0  ->  y = -3 when n=(0,1,0), d=3.
  // Ambient is set to black so that shadowed pixels are fully dark.
  GIVEN(EngineFeatureTest, "a red floor plane at y=-3 with no ambient") {
    auto plane = std::make_shared<Plane>(Vector3d(0, 1, 0), 3);
    plane->setMaterial(
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord(1, 0, 0))));
    test->add(plane);
    test->scene()->setAmbient(Colord(0, 0, 0));
  }

  // Uses projectPoint to convert known world positions to raster coordinates
  // at render time, so the assertion survives buffer-size changes.
  THEN(EngineFeatureTest, "the shadow boundary matches the tangent prediction") {
    // Shadow centre: L=(0,5,0) -> C=(0,1,0) -> floor y=-3 at t=8 -> (0,-3,0)
    const Vector3d shadowCenter(0.0, -3.0, 0.0);
    // Tangent prediction puts the floor boundary at x ~= 2.07. Sample just
    // inside and just outside that edge to pin the geometric angle.
    const Vector3d shadowedInsideBoundary(1.8, -3.0, 0.0);
    const Vector3d litOutsideBoundary(2.4, -3.0, 0.0);

    auto cam = test->camera();
    const Vector2d centerPixel = cam->projectPoint(shadowCenter);
    const Vector2d insidePixel = cam->projectPoint(shadowedInsideBoundary);
    const Vector2d outsidePixel = cam->projectPoint(litOutsideBoundary);

    ASSERT_FALSE(std::isnan(centerPixel.x())) << "shadowCenter is behind the camera";
    ASSERT_FALSE(std::isnan(insidePixel.x())) << "shadowedInsideBoundary is behind the camera";
    ASSERT_FALSE(std::isnan(outsidePixel.x())) << "litOutsideBoundary is behind the camera";

    const int cx = static_cast<int>(centerPixel.x() + 0.5);
    const int cy = static_cast<int>(centerPixel.y() + 0.5);
    const int ix = static_cast<int>(insidePixel.x() + 0.5);
    const int iy = static_cast<int>(insidePixel.y() + 0.5);
    const int ox = static_cast<int>(outsidePixel.x() + 0.5);
    const int oy = static_cast<int>(outsidePixel.y() + 0.5);

    ASSERT_GE(cx, 0);
    ASSERT_LT(cx, 200);
    ASSERT_GE(cy, 0);
    ASSERT_LT(cy, 150);
    ASSERT_GE(ix, 0);
    ASSERT_LT(ix, 200);
    ASSERT_GE(iy, 0);
    ASSERT_LT(iy, 150);
    ASSERT_GE(ox, 0);
    ASSERT_LT(ox, 200);
    ASSERT_GE(oy, 0);
    ASSERT_LT(oy, 150);

    // Ambient=0 + light blocked by sphere: completely dark at shadow centre
    EXPECT_EQ(0u, test->colorAt(cx, cy))
      << "pixel (" << cx << "," << cy << ") at shadow centre should be black";
    EXPECT_EQ(0u, test->colorAt(ix, iy))
      << "pixel (" << ix << "," << iy << ") just inside the tangent boundary should be black";

    // Red floor illuminated by white light: positive red channel just outside
    // the predicted boundary.
    EXPECT_GT(test->colorAt(ox, oy) >> 16, 0u)
      << "pixel (" << ox << "," << oy << ") just outside the tangent boundary should have red";
  }

  TEST_F(PointLightTest, ShadowBoundaryFallsAtGeometricallyPredictedLocation) {
    given("a point light at (0, 5, 0)");
    given("a unit sphere occluder at (0, 1, 0)");
    given("a red floor plane at y=-3 with no ambient");
    when("i look at the origin");
    then("the shadow boundary matches the tangent prediction");
  }

} // namespace PointLightTest
