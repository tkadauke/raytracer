#include "gtest.h"
#include "cameras/OrthographicCamera.h"
#include "Raytracer.h"
#include "surfaces/Scene.h"
#include "Buffer.h"

namespace OrthographicCameraTest {
  using namespace ::testing;
  
  TEST(OrthographicCamera, ShouldConstructWithoutParameters) {
    OrthographicCamera camera;
  }
  
  TEST(OrthographicCamera, ShouldConstructWithParameters) {
    OrthographicCamera camera(Vector3d(0, 0, 1), Vector3d::null);
  }
  
  TEST(OrthographicCamera, ShouldRender) {
    OrthographicCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    Scene scene(Colord::white);
    Raytracer raytracer(&scene);
    Buffer buffer(1, 1);
    camera.render(&raytracer, buffer);
    ASSERT_EQ(Colord::white, buffer[0][0]);
  }
}
