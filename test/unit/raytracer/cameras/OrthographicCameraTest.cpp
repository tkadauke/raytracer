#include "gtest.h"
#include "raytracer/cameras/OrthographicCamera.h"
#include "raytracer/Raytracer.h"
#include "raytracer/primitives/Scene.h"
#include "core/Buffer.h"

namespace OrthographicCameraTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(OrthographicCamera, ShouldConstructWithoutParameters) {
    OrthographicCamera camera;
  }
  
  TEST(OrthographicCamera, ShouldConstructWithParameters) {
    OrthographicCamera camera(Vector3d(0, 0, 1), Vector3d::null());
  }
  
  TEST(OrthographicCamera, ShouldRender) {
    OrthographicCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Scene scene(Colord::white());
    auto raytracer = std::make_shared<Raytracer>(&scene);
    Buffer<unsigned int> buffer(1, 1);
    camera.render(raytracer, buffer);
    ASSERT_EQ(Colord::white().rgb(), buffer[0][0]);
  }
  
  TEST(OrthographicCamera, ShouldGetRayForPixelWithUninitializedViewPlane) {
    OrthographicCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Rayd ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, 0), ray.origin());
    ASSERT_EQ(Vector3d(0, 0, 1), ray.direction());
  }
  
  TEST(OrthographicCamera, ShouldGetRayForPixelWithInitializedViewPlane) {
    OrthographicCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    auto raytracer = std::make_shared<Raytracer>(new Scene(Colord::white()));
    Buffer<unsigned int> buffer(1, 1);
    camera.render(raytracer, buffer);
    
    Rayd ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, 0), ray.origin());
    ASSERT_EQ(Vector3d(0, 0, 1), ray.direction());
  }
}
