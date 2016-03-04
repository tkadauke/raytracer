#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "raytracer/primitives/Instance.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace InstanceTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(Instance, ShouldReturnChildPrimitiveIfTransformedRayIntersects) {
    auto primitive = std::make_shared<MockPrimitive>();
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoint(HitPoint(1.0, Vector3d(), Vector3d(1, 0, 0))),
        Return(primitive.get())
      )
    );
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    auto result = instance.intersect(ray, hitPoints);
    
    ASSERT_EQ(primitive.get(), result);
  }
  
  TEST(Instance, ShouldNotReturnAnyPrimitiveIfThereIsNoIntersection) {
    auto primitive = std::make_shared<MockPrimitive>();
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersect(_, _)).WillOnce(Return(static_cast<Primitive*>(nullptr)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    auto result = instance.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
  }
  
  TEST(Instance, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    auto primitive = std::make_shared<MockPrimitive>();
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersects(_)).WillOnce(Return(true));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    ASSERT_TRUE(instance.intersects(ray));
  }

  TEST(Instance, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    auto primitive = std::make_shared<MockPrimitive>();
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersects(_)).WillOnce(Return(false));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    ASSERT_FALSE(instance.intersects(ray));
  }
  
  TEST(Instance, ShouldReturnBoundingBox) {
    auto primitive = std::make_shared<MockPrimitive>();
    Instance instance(primitive);
    instance.setMatrix(Matrix3d::scale(2));
    EXPECT_CALL(*primitive, boundingBox()).WillOnce(Return(BoundingBox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1))));
    
    BoundingBox expected(Vector3d(-2, -2, -2), Vector3d(2, 2, 2));
    ASSERT_EQ(expected, instance.boundingBox());
  }
}
