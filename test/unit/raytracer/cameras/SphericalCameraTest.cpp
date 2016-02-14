#include "gtest.h"
#include "raytracer/cameras/SphericalCamera.h"
#include "raytracer/Raytracer.h"
#include "raytracer/primitives/Scene.h"
#include "core/Buffer.h"

namespace SphericalCameraTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(SphericalCamera, ShouldConstructWithoutParameters) {
    SphericalCamera camera;
    ASSERT_NEAR(180, camera.horizontalFieldOfView().degrees(), 0.001);
    ASSERT_NEAR(120, camera.verticalFieldOfView().degrees(), 0.001);
  }
  
  TEST(SphericalCamera, ShouldConstructWithParameters) {
    SphericalCamera camera(Vector3d(0, 0, 1), Vector3d::null());
    ASSERT_NEAR(180, camera.horizontalFieldOfView().degrees(), 0.001);
    ASSERT_NEAR(120, camera.verticalFieldOfView().degrees(), 0.001);
  }
  
  TEST(SphericalCamera, ShouldConstructWithFieldOfViews) {
    SphericalCamera camera(Angled::fromDegrees(200), Angled::fromDegrees(90));
    ASSERT_NEAR(200, camera.horizontalFieldOfView().degrees(), 0.001);
    ASSERT_NEAR(90, camera.verticalFieldOfView().degrees(), 0.001);
  }
  
  TEST(SphericalCamera, ShouldSetHorizontalFieldOfView) {
    SphericalCamera camera;
    camera.setHorizontalFieldOfView(Angled::fromDegrees(200));
    ASSERT_NEAR(200, camera.horizontalFieldOfView().degrees(), 0.001);
  }
  
  TEST(SphericalCamera, ShouldSetVerticalFieldOfView) {
    SphericalCamera camera;
    camera.setVerticalFieldOfView(Angled::fromDegrees(140));
    ASSERT_NEAR(140, camera.verticalFieldOfView().degrees(), 0.001);
  }
  
  TEST(SphericalCamera, ShouldRender) {
    SphericalCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Scene scene(Colord::white());
    Raytracer raytracer(&scene);
    Buffer<unsigned int> buffer(1, 1);
    camera.render(&raytracer, buffer);
    ASSERT_EQ(Colord::white().rgb(), buffer[0][0]);
  }
  
  TEST(SphericalCamera, ShouldGetRayForPixelWithUninitializedViewPlane) {
    SphericalCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Ray ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -6), ray.origin());
    ASSERT_TRUE(ray.direction().isUndefined());
  }
  
  TEST(SphericalCamera, ShouldGetRayForPixelWithInitializedViewPlane) {
    SphericalCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Raytracer raytracer(new Scene(Colord::white()));
    Buffer<unsigned int> buffer(1, 1);
    camera.render(&raytracer, buffer);

    Ray ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -6), ray.origin());
    ASSERT_TRUE(ray.direction().isUndefined());
  }
}
