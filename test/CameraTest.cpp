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
  
  TEST(Camera, ShouldReturnMatrixWithCorrectTranslation) {
    Camera camera(Vector3d(4, 3, 2), Vector3d::null);
    ASSERT_EQ(4, camera.matrix()[0][3]);
    ASSERT_EQ(3, camera.matrix()[1][3]);
    ASSERT_EQ(2, camera.matrix()[2][3]);
  }
}
