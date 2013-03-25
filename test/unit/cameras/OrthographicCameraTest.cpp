#include "gtest.h"
#include "cameras/OrthographicCamera.h"
#include "Raytracer.h"
#include "primitives/Scene.h"
#include "Buffer.h"

namespace OrthographicCameraTest {
  using namespace ::testing;
  
  TEST(OrthographicCamera, ShouldConstructWithoutParameters) {
    OrthographicCamera camera;
  }
  
  TEST(OrthographicCamera, ShouldConstructWithParameters) {
    OrthographicCamera camera(Vector3d(0, 0, 1), Vector3d::null());
  }
  
  TEST(OrthographicCamera, ShouldRender) {
    OrthographicCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Scene scene(Colord::white());
    Raytracer raytracer(&scene);
    Buffer<unsigned int> buffer(1, 1);
    camera.render(&raytracer, buffer);
    ASSERT_EQ(Colord::white().rgb(), buffer[0][0]);
  }
  
  TEST(OrthographicCamera, ShouldGetRayForPixelWithUninitializedViewPlane) {
    OrthographicCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Ray ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, 0), ray.origin());
    ASSERT_EQ(Vector3d(0, 0, 1), ray.direction());
  }
  
  TEST(OrthographicCamera, ShouldGetRayForPixelWithInitializedViewPlane) {
    OrthographicCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Raytracer raytracer(new Scene(Colord::white()));
    Buffer<unsigned int> buffer(1, 1);
    camera.render(&raytracer, buffer);
    
    Ray ray = camera.rayForPixel(0, 0);
    ASSERT_EQ(Vector3d(0, 0, 0), ray.origin());
    ASSERT_EQ(Vector3d(0, 0, 1), ray.direction());
  }
}
