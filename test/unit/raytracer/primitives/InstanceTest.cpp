#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "raytracer/State.h"
#include "raytracer/primitives/Instance.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace InstanceTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(Instance, ShouldReturnChildPrimitiveIfTransformedRayIntersects) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersect(_, _, _)).WillOnce(
      DoAll(
        AddHitPoint(HitPoint(primitive.get(), 1.0, Vector3d(), Vector3d(1, 0, 0))),
        Return(primitive.get())
      )
    );
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto result = instance.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(primitive.get(), result);
  }
  
  TEST(Instance, ShouldNotReturnAnyPrimitiveIfThereIsNoIntersection) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersect(_, _, _)).WillOnce(Return(static_cast<Primitive*>(nullptr)));
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto result = instance.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(nullptr, result);
  }
  
  TEST(Instance, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersects(_, _)).WillOnce(Return(true));
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    ASSERT_TRUE(instance.intersects(ray, state));
  }

  TEST(Instance, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersects(_, _)).WillOnce(Return(false));
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    ASSERT_FALSE(instance.intersects(ray, state));
  }
  
  TEST(Instance, ShouldReturnFarthestPoint) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    instance.setMatrix(Matrix3d::scale(2));
    EXPECT_CALL(*primitive, farthestPoint(_)).WillOnce(Return(Vector3d(1, 1, 1)));
    
    Vector3d expected(2, 2, 2);
    ASSERT_EQ(expected, instance.farthestPoint(Vector3d::one()));
  }
  
  TEST(Instance, ShouldReturnBoundingBox) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    instance.setMatrix(Matrix3d::scale(2));
    EXPECT_CALL(*primitive, calculateBoundingBox()).WillOnce(Return(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1))));
    
    BoundingBoxd expected(Vector3d(-2, -2, -2), Vector3d(2, 2, 2));
    ASSERT_EQ(expected, instance.boundingBox());
  }
}
