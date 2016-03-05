#include "gtest.h"
#include "raytracer/cameras/Camera.h"
#include "raytracer/viewplanes/ViewPlane.h"
#include "test/mocks/raytracer/MockViewPlane.h"

namespace CameraTest {
  using namespace ::testing;
  using namespace raytracer;
  
  class ConcreteCamera : public Camera {
  public:
    inline ConcreteCamera()
      : Camera()
    {
    }
    
    inline ConcreteCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target)
    {
    }
    
    inline virtual void render(std::shared_ptr<Raytracer>, Buffer<unsigned int>&, const Recti&) {
      // noop
    }
    
    inline virtual Ray rayForPixel(double, double) {
      return Ray::undefined();
    }
  };
  
  TEST(Camera, ShouldConstructWithoutParameters) {
    ConcreteCamera camera;
  }
  
  TEST(Camera, ShouldConstructWithParameters) {
    ConcreteCamera camera(Vector3d(0, 0, 1), Vector3d::null());
  }
  
  TEST(Camera, ShouldDeleteViewPlaneOnDestruct) {
    auto plane = std::make_shared<MockViewPlane>();
    auto camera = new ConcreteCamera;
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
    auto plane = std::make_shared<ViewPlane>();
    camera.setViewPlane(plane);
    ASSERT_EQ(plane, camera.viewPlane());
  }
  
  TEST(Camera, ShouldDeleteOldViewPlaneWhenNewIsSet) {
    ConcreteCamera camera;
    auto plane = std::make_shared<testing::MockViewPlane>();
    EXPECT_CALL(*plane, destructorCall());
    camera.setViewPlane(plane);
    camera.setViewPlane(std::make_shared<ViewPlane>());
  }
  
  TEST(Camera, ShouldReturnDefaultViewPlane) {
    ConcreteCamera camera;
    ASSERT_NE(static_cast<std::shared_ptr<ViewPlane>>(0), camera.viewPlane());
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
