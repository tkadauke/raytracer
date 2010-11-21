#include "gtest.h"
#include "Camera.h"

namespace CameraTest {
  TEST(Camera, ShouldReturnMatrix) {
    Camera camera(Vector3d(0, 0, -1), Vector3d::null);
    Matrix4d expected(
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, -1,
      0, 0, 0, 1
    );
    ASSERT_EQ(expected, camera.matrix());
  }
}
