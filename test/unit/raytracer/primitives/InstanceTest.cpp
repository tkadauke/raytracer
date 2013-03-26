#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "raytracer/primitives/Instance.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace InstanceTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(Instance, ShouldReturnChildPrimitiveIfTransformedRayIntersects) {
    MockPrimitive* primitive = new MockPrimitive;
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(1.0, Vector3d(), Vector3d(1, 0, 0))), Return(primitive)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Primitive* result = instance.intersect(ray, hitPoints);
    
    ASSERT_EQ(&instance, result);
    
    delete primitive;
  }
  
  TEST(Instance, ShouldNotReturnAnyPrimitiveIfThereIsNoIntersection) {
    MockPrimitive* primitive = new MockPrimitive;
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersect(_, _)).WillOnce(Return(static_cast<Primitive*>(0)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Primitive* result = instance.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
    
    delete primitive;
  }
  
  TEST(Instance, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    MockPrimitive* primitive = new MockPrimitive;
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersects(_)).WillOnce(Return(true));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    ASSERT_TRUE(instance.intersects(ray));
    
    delete primitive;
  }

  TEST(Instance, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    MockPrimitive* primitive = new MockPrimitive;
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersects(_)).WillOnce(Return(false));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    ASSERT_FALSE(instance.intersects(ray));
    
    delete primitive;
  }
  
  TEST(Instance, ShouldReturnBoundingBox) {
    MockPrimitive* primitive = new MockPrimitive;
    Instance instance(primitive);
    instance.setMatrix(Matrix3d::scale(2));
    EXPECT_CALL(*primitive, boundingBox()).WillOnce(Return(BoundingBox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1))));
    
    BoundingBox expected(Vector3d(-2, -2, -2), Vector3d(2, 2, 2));
    ASSERT_EQ(expected, instance.boundingBox());
    
    delete primitive;
  }
}
