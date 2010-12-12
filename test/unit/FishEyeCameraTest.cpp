#include "gtest.h"
#include "FishEyeCamera.h"
#include "Raytracer.h"
#include "Scene.h"
#include "Buffer.h"

namespace FishEyeCameraTest {
  using namespace ::testing;
  
  TEST(FishEyeCamera, ShouldConstructWithoutParameters) {
    FishEyeCamera camera;
    ASSERT_EQ(120, camera.fieldOfView());
  }
  
  TEST(FishEyeCamera, ShouldConstructWithParameters) {
    FishEyeCamera camera(Vector3d(0, 0, 1), Vector3d::null);
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
    FishEyeCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    Scene scene(Colord::white);
    Raytracer raytracer(&scene);
    Buffer buffer(1, 1);
    camera.render(&raytracer, buffer);
    // This is black because of the black rounded border around fish eye images
    ASSERT_EQ(Colord::black, buffer[0][0]);
  }
}
