#include "gtest.h"
#include "raytracer/cameras/Camera.h"
#include "raytracer/viewplanes/ViewPlane.h"
#include "test/mocks/raytracer/MockViewPlane.h"

namespace CameraTest {
  using namespace ::testing;
  
  class ConcreteCamera : public Camera {
  public:
    ConcreteCamera() : Camera() {}
    ConcreteCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target) {}
    virtual void render(Raytracer*, Buffer<unsigned int>&, const Rect&) {}
    virtual Ray rayForPixel(int x, int y) {}
  };
  
  TEST(Camera, ShouldConstructWithoutParameters) {
    ConcreteCamera camera;
  }
  
  TEST(Camera, ShouldConstructWithParameters) {
    ConcreteCamera camera(Vector3d(0, 0, 1), Vector3d::null());
  }
  
  TEST(Camera, ShouldDeleteViewPlaneOnDestruct) {
    MockViewPlane* plane = new MockViewPlane;
    ConcreteCamera* camera = new ConcreteCamera;
    EXPECT_CALL(*plane, destructorCall());
    camera->setViewPlane(plane);
    delete camera;
  }
  
  TEST(Camera, ShouldReturnMatrix) {
    ConcreteCamera camera(Vector3d(0, 0, -1), Vector3d::null());
    Matrix4d expected(
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, -1,
      0, 0, 0, 1
    );
    ASSERT_EQ(expected, camera.matrix());
  }
  
  TEST(Camera, ShouldReturnMatrixWithCorrectTranslation) {
    ConcreteCamera camera(Vector3d(4, 3, 2), Vector3d::null());
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
  
  TEST(Camera, ShouldSetViewPlane) {
    ConcreteCamera camera;
    ViewPlane* plane = new ViewPlane;
    camera.setViewPlane(plane);
    ASSERT_EQ(plane, camera.viewPlane());
  }
  
  TEST(Camera, ShouldDeleteOldViewPlaneWhenNewIsSet) {
    ConcreteCamera camera;
    testing::MockViewPlane* plane = new testing::MockViewPlane;
    EXPECT_CALL(*plane, destructorCall());
    camera.setViewPlane(plane);
    camera.setViewPlane(new ViewPlane);
  }
  
  TEST(Camera, ShouldReturnDefaultViewPlane) {
    ConcreteCamera camera;
    ASSERT_NE(static_cast<ViewPlane*>(0), camera.viewPlane());
  }
  
  TEST(Camera, ShouldNotBeCancelledAfterConstruction) {
    ASSERT_FALSE(ConcreteCamera().isCancelled());
    ASSERT_FALSE(ConcreteCamera(Vector3d(), Vector3d()).isCancelled());
  }
  
  TEST(Camera, ShouldBeCancelledAfterCancellation) {
    ConcreteCamera camera;
    camera.cancel();
    ASSERT_TRUE(camera.isCancelled());
  }
  
  TEST(Camera, ShouldUncancel) {
    ConcreteCamera camera;
    camera.cancel();
    camera.uncancel();
    ASSERT_FALSE(camera.isCancelled());
  }
}
