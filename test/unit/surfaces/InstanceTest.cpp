#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "surfaces/Instance.h"
#include "test/mocks/MockSurface.h"

namespace InstanceTest {
  using namespace ::testing;
  
  TEST(Instance, ShouldReturnChildSurfaceIfTransformedRayIntersects) {
    MockSurface* surface = new MockSurface;
    Instance instance(surface);
    EXPECT_CALL(*surface, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(1.0, Vector3d(), Vector3d())), Return(surface)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* result = instance.intersect(ray, hitPoints);
    
    ASSERT_EQ(&instance, result);
    
    delete surface;
  }
  
  TEST(Instance, ShouldNotReturnAnySurfaceIfThereIsNoIntersection) {
    MockSurface* surface = new MockSurface;
    Instance instance(surface);
    EXPECT_CALL(*surface, intersect(_, _)).WillOnce(Return(static_cast<Surface*>(0)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* result = instance.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
    
    delete surface;
  }
  
  TEST(Instance, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    MockSurface* surface = new MockSurface;
    Instance instance(surface);
    EXPECT_CALL(*surface, intersects(_)).WillOnce(Return(true));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    ASSERT_TRUE(instance.intersects(ray));
    
    delete surface;
  }

  TEST(Instance, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    MockSurface* surface = new MockSurface;
    Instance instance(surface);
    EXPECT_CALL(*surface, intersects(_)).WillOnce(Return(false));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    ASSERT_FALSE(instance.intersects(ray));
    
    delete surface;
  }
  
  TEST(Instance, ShouldReturnBoundingBox) {
    MockSurface* surface = new MockSurface;
    Instance instance(surface);
    instance.setMatrix(Matrix3d::scale(2));
    EXPECT_CALL(*surface, boundingBox()).WillOnce(Return(BoundingBox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1))));
    
    BoundingBox expected(Vector3d(-2, -2, -2), Vector3d(2, 2, 2));
    ASSERT_EQ(expected, instance.boundingBox());
    
    delete surface;
  }
}
