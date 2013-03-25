#include "gtest.h"
#include "cameras/PinholeCamera.h"
#include "Raytracer.h"
#include "primitives/Scene.h"
#include "Buffer.h"

namespace PinholeCameraTest {
  using namespace ::testing;
  
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
    Scene scene(Colord::white());
    Raytracer raytracer(&scene);
    Buffer<unsigned int> buffer(1, 1);
    camera.render(&raytracer, buffer);
    ASSERT_EQ(Colord::white().rgb(), buffer[0][0]);
  }
  
  TEST(PinholeCamera, ShouldSetViewplanePixelSize) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Scene scene(Colord::white());
    Raytracer raytracer(&scene);
    Buffer<unsigned int> buffer(1, 1);

    camera.setZoom(2);
    camera.render(&raytracer, buffer);
    ASSERT_EQ(0.5, camera.viewPlane()->pixelSize());
  }
  
  TEST(PinholeCamera, ShouldGetRayForPixelWithUninitializedViewPlane) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Ray ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -6), ray.origin());
    ASSERT_EQ(Vector3d(0, 0, 1), ray.direction());
  }
  
  TEST(PinholeCamera, ShouldGetRayForPixelWithInitializedViewPlane) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Raytracer raytracer(new Scene(Colord::white()));
    Buffer<unsigned int> buffer(1, 1);
    camera.render(&raytracer, buffer);

    Ray ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -6), ray.origin());
    ASSERT_EQ(Vector3d(0, 0, 1), ray.direction());
  }
}
