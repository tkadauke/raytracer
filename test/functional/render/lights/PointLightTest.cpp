#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "raytracer/primitives/Sphere.h"
#include "raytracer/primitives/Plane.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/lights/PointLight.h"
#include "raytracer/materials/MatteMaterial.h"
#include "raytracer/textures/ConstantColorTexture.h"
#include "core/Color.h"

#include <cmath>

using namespace testing;
using namespace raytracer;

// Scene geometry for shadow-boundary tests:
//
//   Light:   (0, 5, 0)  — white point light above the scene
//   Sphere:  centre (0, 1, 0), radius 1  — opaque occluder
//   Floor:   Plane(normal=(0,1,0), d=3) → equation y = -3
//   Camera:  (0,0,-5) → (0,0,0)  (lookAtOrigin)
//
// Shadow-centre on the floor:
//   Line from L=(0,5,0) through C=(0,1,0) → direction (0,-1,0)
//   Hits y=-3 at t=8: world point (0,-3,0) → pixel ~(100,38)
//
// Umbra boundary (2-D tangent from light to sphere, then to floor):
//   |LC|=4, r=1  → sin θ=¼, boundary x ≈ ±2.07 at y=-3
//
// Lit test point: (4,-3,0) is well outside the shadow cone → pixel ~(150,38)

namespace PointLightTest {

struct PointLightTest : public RaytracerFeatureTest {};

GIVEN(RaytracerFeatureTest, "a point light at (0, 5, 0)") {
  test->scene()->addLight(
    std::make_shared<PointLight>(Vector3d(0, 5, 0), Colord::white())
  );
}

// Sphere needs no material: shadow rays only need geometry; camera rays
// that do hit the sphere return black (harmless for the floor pixels tested).
GIVEN(RaytracerFeatureTest, "a unit sphere occluder at (0, 1, 0)") {
  test->add(std::make_shared<Sphere>(Vector3d(0, 1, 0), 1.0));
}

// Plane equation: n·p + d = 0  →  y = -3 when n=(0,1,0), d=3.
// Ambient is set to black so that shadowed pixels are fully dark.
GIVEN(RaytracerFeatureTest, "a red floor plane at y=-3 with no ambient") {
  auto plane = std::make_shared<Plane>(Vector3d(0, 1, 0), 3);
  plane->setMaterial(std::make_shared<MatteMaterial>(
    std::make_shared<ConstantColorTexture>(Colord(1, 0, 0))
  ));
  test->add(plane);
  test->scene()->setAmbient(Colord(0, 0, 0));
}

// Uses projectPoint to convert known world positions to raster coordinates
// at render time, so the assertion survives buffer-size changes.
THEN(RaytracerFeatureTest, "the shadow centre pixel is dark and a lit pixel outside the cone is bright") {
  // Shadow centre: L=(0,5,0) → C=(0,1,0) → floor y=-3 at t=8 → (0,-3,0)
  const Vector3d shadowCenter(0.0, -3.0, 0.0);
  // Lit point: x=4 >> umbra boundary x≈2.07, same floor depth
  const Vector3d litPoint(4.0, -3.0, 0.0);

  auto cam = test->camera();
  Vector2d sp = cam->projectPoint(shadowCenter);
  Vector2d lp = cam->projectPoint(litPoint);

  ASSERT_FALSE(std::isnan(sp.x())) << "shadowCenter is behind the camera";
  ASSERT_FALSE(std::isnan(lp.x())) << "litPoint is behind the camera";

  const int sx = static_cast<int>(sp.x() + 0.5);
  const int sy = static_cast<int>(sp.y() + 0.5);
  const int lx = static_cast<int>(lp.x() + 0.5);
  const int ly = static_cast<int>(lp.y() + 0.5);

  ASSERT_GE(sx, 0); ASSERT_LT(sx, 200);
  ASSERT_GE(sy, 0); ASSERT_LT(sy, 150);
  ASSERT_GE(lx, 0); ASSERT_LT(lx, 200);
  ASSERT_GE(ly, 0); ASSERT_LT(ly, 150);

  // Ambient=0 + light blocked by sphere → completely dark at shadow centre
  EXPECT_EQ(0u, test->colorAt(sx, sy))
    << "pixel (" << sx << "," << sy << ") at shadow centre should be black";

  // Red floor illuminated by white light → positive red channel at lit point
  EXPECT_GT(test->colorAt(lx, ly) >> 16, 0u)
    << "pixel (" << lx << "," << ly << ") at lit floor point should have red";
}

TEST_F(PointLightTest, ShadowBoundaryFallsAtGeometricallyPredictedLocation) {
  given("a point light at (0, 5, 0)");
  given("a unit sphere occluder at (0, 1, 0)");
  given("a red floor plane at y=-3 with no ambient");
  when("i look at the origin");
  then("the shadow centre pixel is dark and a lit pixel outside the cone is bright");
}

} // namespace PointLightTest
