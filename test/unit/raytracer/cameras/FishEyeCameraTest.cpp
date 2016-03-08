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
    ASSERT_NEAR(120, camera.fieldOfView().degrees(), 0.001);
  }
  
  TEST(FishEyeCamera, ShouldConstructWithParameters) {
    FishEyeCamera camera(Vector3d(0, 0, 1), Vector3d::null());
    ASSERT_NEAR(120, camera.fieldOfView().degrees(), 0.001);
  }
  
  TEST(FishEyeCamera, ShouldConstructWithFieldOfView) {
    FishEyeCamera camera(Angled::fromDegrees(360));
    ASSERT_NEAR(360, camera.fieldOfView().degrees(), 0.001);
  }
  
  TEST(FishEyeCamera, ShouldSetFieldOfView) {
    FishEyeCamera camera;
    camera.setFieldOfView(Angled::fromDegrees(200));
    ASSERT_NEAR(200, camera.fieldOfView().degrees(), 0.001);
  }
  
  TEST(FishEyeCamera, ShouldRender) {
    FishEyeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Scene scene(Colord::white());
    auto raytracer = std::make_shared<Raytracer>(&scene);
    Buffer<unsigned int> buffer(1, 1);
    camera.render(raytracer, buffer);
    // This is black because of the black rounded border around fish eye images
    ASSERT_EQ(Colord::black().rgb(), buffer[0][0]);
  }
  
  TEST(FishEyeCamera, ShouldGetRayForPixelWithUninitializedViewPlane) {
    FishEyeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Rayd ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -1), ray.origin());
    ASSERT_TRUE(ray.direction().isUndefined());
  }
  
  TEST(FishEyeCamera, ShouldGetRayForPixelWithInitializedViewPlane) {
    FishEyeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    auto raytracer = std::make_shared<Raytracer>(new Scene(Colord::white()));
    Buffer<unsigned int> buffer(1, 1);
    camera.render(raytracer, buffer);

    Rayd ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, -1), ray.origin());
    ASSERT_TRUE(ray.direction().isUndefined());
  }
}
