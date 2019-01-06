#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "raytracer/primitives/Union.h"
#include "raytracer/materials/MatteMaterial.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace UnionTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(Union, ShouldReturnClosestPrimitiveForUnion) {
    Union u;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1.get(), intersect(_, _, _)).WillOnce(
      DoAll(
        AddHitPoint(HitPoint(primitive1.get(), 1.0, Vector3d(), Vector3d())),
        Return(primitive1.get())
      )
    );
    EXPECT_CALL(*primitive2.get(), intersect(_, _, _)).WillOnce(
      Return(static_cast<Primitive*>(nullptr))
    );
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto result = u.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(primitive1.get(), result);
  }

  TEST(Union, ShouldReturnSelfIfDifferenceHasMaterial) {
    Union u;
    u.setMaterial(std::make_shared<MatteMaterial>());
    
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1.get(), intersect(_, _, _)).WillOnce(
      DoAll(
        AddHitPoint(HitPoint(primitive1.get(), 1.0, Vector3d(), Vector3d())),
        Return(primitive1.get())
      )
    );
    EXPECT_CALL(*primitive2.get(), intersect(_, _, _)).WillOnce(
      Return(static_cast<Primitive*>(nullptr))
    );
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto result = u.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(&u, result);
  }
  
  TEST(Union, ShouldNotReturnAnyPrimitiveIfThereIsNoIntersection) {
    Union u;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1.get(), intersect(_, _, _)).WillOnce(Return(static_cast<Primitive*>(nullptr)));
    EXPECT_CALL(*primitive2.get(), intersect(_, _, _)).WillOnce(Return(static_cast<Primitive*>(nullptr)));
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto result = u.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, result);
  }
  
  TEST(Union, ShouldBuildUnionOfHitPoints) {
    Union u;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1.get(), intersect(_, _, _)).WillOnce(
      DoAll(
        AddHitPoint(HitPoint(primitive1.get(), 1.0, Vector3d(), Vector3d())),
        Return(primitive1.get())
      )
    );
    EXPECT_CALL(*primitive2.get(), intersect(_, _, _)).WillOnce(
      DoAll(
        AddHitPoint(HitPoint(primitive2.get(), 5.0, Vector3d(), Vector3d())),
        Return(primitive2.get())
      )
    );
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    u.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(5, hitPoints.max().distance());
  }
  
  TEST(Union, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    Union u;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1, intersects(_, _)).WillOnce(Return(true));
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    ASSERT_TRUE(u.intersects(ray, state));
  }
  
  TEST(Union, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Union u;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1.get(), intersects(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*primitive2.get(), intersects(_, _)).WillOnce(Return(false));
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    ASSERT_FALSE(u.intersects(ray, state));
  }
}
