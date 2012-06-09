#include "gtest.h"
#include "cameras/PinholeCamera.h"
#include "Raytracer.h"
#include "surfaces/Scene.h"
#include "Buffer.h"

namespace PinholeCameraTest {
  using namespace ::testing;
  
  TEST(PinholeCamera, ShouldConstructWithoutParameters) {
    PinholeCamera camera;
    ASSERT_EQ(5, camera.distance());
  }
  
  TEST(PinholeCamera, ShouldConstructWithParameters) {
    PinholeCamera camera(Vector3d(0, 0, 1), Vector3d::null());
    ASSERT_EQ(5, camera.distance());
  }
  
  TEST(PinholeCamera, ShouldSetDistance) {
    PinholeCamera camera;
    camera.setDistance(20);
    ASSERT_EQ(20, camera.distance());
  }
  
  TEST(PinholeCamera, ShouldRender) {
    PinholeCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Scene scene(Colord::white());
    Raytracer raytracer(&scene);
    Buffer<unsigned int> buffer(1, 1);
    camera.render(&raytracer, buffer);
    ASSERT_EQ(Colord::white().rgb(), buffer[0][0]);
  }
}
