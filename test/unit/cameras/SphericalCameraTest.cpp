#include "gtest.h"
#include "cameras/SphericalCamera.h"
#include "Raytracer.h"
#include "surfaces/Scene.h"
#include "Buffer.h"

namespace SphericalCameraTest {
  using namespace ::testing;
  
  TEST(SphericalCamera, ShouldConstructWithoutParameters) {
    SphericalCamera camera;
    ASSERT_EQ(180, camera.horizontalFieldOfView());
    ASSERT_EQ(120, camera.verticalFieldOfView());
  }
  
  TEST(SphericalCamera, ShouldConstructWithParameters) {
    SphericalCamera camera(Vector3d(0, 0, 1), Vector3d::null());
    ASSERT_EQ(180, camera.horizontalFieldOfView());
    ASSERT_EQ(120, camera.verticalFieldOfView());
  }
  
  TEST(SphericalCamera, ShouldConstructWithFieldOfViews) {
    SphericalCamera camera(200, 90);
    ASSERT_EQ(200, camera.horizontalFieldOfView());
    ASSERT_EQ(90, camera.verticalFieldOfView());
  }
  
  TEST(SphericalCamera, ShouldSetHorizontalFieldOfView) {
    SphericalCamera camera;
    camera.setHorizontalFieldOfView(200);
    ASSERT_EQ(200, camera.horizontalFieldOfView());
  }
  
  TEST(SphericalCamera, ShouldSetVerticalFieldOfView) {
    SphericalCamera camera;
    camera.setVerticalFieldOfView(140);
    ASSERT_EQ(140, camera.verticalFieldOfView());
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
