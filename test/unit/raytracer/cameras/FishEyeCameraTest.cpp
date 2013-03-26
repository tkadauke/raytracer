#include "gtest.h"
#include "raytracer/cameras/FishEyeCamera.h"
#include "raytracer/Raytracer.h"
#include "raytracer/primitives/Scene.h"
#include "core/Buffer.h"

namespace FishEyeCameraTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(FishEyeCamera, ShouldConstructWithoutParameters) {
    FishEyeCamera camera;
    ASSERT_EQ(120, camera.fieldOfView());
  }
  
  TEST(FishEyeCamera, ShouldConstructWithParameters) {
    FishEyeCamera camera(Vector3d(0, 0, 1), Vector3d::null());
    ASSERT_EQ(120, camera.fieldOfView());
  }
  
  TEST(FishEyeCamera, ShouldConstructWithFieldOfView) {
    FishEyeCamera camera(360);
    ASSERT_EQ(360, camera.fieldOfView());
  }
  
  TEST(FishEyeCamera, ShouldSetFieldOfView) {
    FishEyeCamera camera;
    camera.setFieldOfView(200);
    ASSERT_EQ(200, camera.fieldOfView());
  }
  
  TEST(FishEyeCamera, ShouldRender) {
    FishEyeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Scene scene(Colord::white());
    Raytracer raytracer(&scene);
    Buffer<unsigned int> buffer(1, 1);
    camera.render(&raytracer, buffer);
    // This is black because of the black rounded border around fish eye images
    ASSERT_EQ(Colord::black().rgb(), buffer[0][0]);
  }
  
  TEST(FishEyeCamera, ShouldGetRayForPixelWithUninitializedViewPlane) {
    FishEyeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Ray ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -1), ray.origin());
    ASSERT_TRUE(ray.direction().isUndefined());
  }
  
  TEST(FishEyeCamera, ShouldGetRayForPixelWithInitializedViewPlane) {
    FishEyeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Raytracer raytracer(new Scene(Colord::white()));
    Buffer<unsigned int> buffer(1, 1);
    camera.render(&raytracer, buffer);

    Ray ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -1), ray.origin());
    ASSERT_TRUE(ray.direction().isUndefined());
  }
}
