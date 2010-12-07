#include "gtest.h"
#include "Camera.h"

namespace CameraTest {
  class ConcreteCamera : public Camera {
  public:
    ConcreteCamera() : Camera() {}
    ConcreteCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target) {}
    virtual void render(Raytracer*, Buffer&) {}
  };
  
  TEST(Camera, ShouldConstructWithoutParameters) {
    ConcreteCamera camera;
  }
  
  TEST(Camera, ShouldConstructWithParameters) {
    ConcreteCamera camera(Vector3d(0, 0, 1), Vector3d::null);
  }
  
  TEST(Camera, ShouldReturnMatrix) {
    ConcreteCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    Matrix4d expected(
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, -1,
      0, 0, 0, 1
    );
    ASSERT_EQ(expected, camera.matrix());
  }
  
  TEST(Camera, ShouldReturnMatrixWithCorrectTranslation) {
    ConcreteCamera camera(Vector3d(4, 3, 2), Vector3d::null);
    ASSERT_EQ(4, camera.matrix()[0][3]);
    ASSERT_EQ(3, camera.matrix()[1][3]);
    ASSERT_EQ(2, camera.matrix()[2][3]);
  }
  
  TEST(Camera, ShouldRecalculateMatrixWhenPositionIsChanged) {
    ConcreteCamera camera;
    camera.setPosition(Vector3d(0, 0, -2));
    Matrix4d expected(
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, -2,
      0, 0, 0, 1
    );
    ASSERT_EQ(expected, camera.matrix());
  }
  
  TEST(Camera, ShouldRecalculateMatrixWhenTargetIsChanged) {
    ConcreteCamera camera;
    camera.setTarget(Vector3d(0, 0, 1));
    Matrix4d expected(
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, 0,
      0, 0, 0, 1
    );
    ASSERT_EQ(expected, camera.matrix());
  }
}
