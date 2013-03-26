#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "raytracer/primitives/Union.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace UnionTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(Union, ShouldReturnSelfIfAnyOfTheChildPrimitivesIntersect) {
    Union u;
    MockPrimitive* primitive1 = new MockPrimitive;
    MockPrimitive* primitive2 = new MockPrimitive;
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(1.0, Vector3d(), Vector3d())), Return(primitive1)));
    EXPECT_CALL(*primitive2, intersect(_, _)).WillOnce(Return(static_cast<Primitive*>(0)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Primitive* result = u.intersect(ray, hitPoints);
    
    ASSERT_EQ(&u, result);
  }
  
  TEST(Union, ShouldNotReturnAnyPrimitiveIfThereIsNoIntersection) {
    Union u;
    MockPrimitive* primitive1 = new MockPrimitive;
    MockPrimitive* primitive2 = new MockPrimitive;
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1, intersect(_, _)).WillOnce(Return(static_cast<Primitive*>(0)));
    EXPECT_CALL(*primitive2, intersect(_, _)).WillOnce(Return(static_cast<Primitive*>(0)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Primitive* result = u.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
  }
  
  TEST(Union, ShouldBuildUnionOfHitPoints) {
    Union u;
    MockPrimitive* primitive1 = new MockPrimitive;
    MockPrimitive* primitive2 = new MockPrimitive;
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(1.0, Vector3d(), Vector3d())), Return(primitive1)));
    EXPECT_CALL(*primitive2, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(5.0, Vector3d(), Vector3d())), Return(primitive2)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    u.intersect(ray, hitPoints);
    
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(5, hitPoints.max().distance());
  }
  
  TEST(Union, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    Union u;
    MockPrimitive* primitive1 = new MockPrimitive;
    MockPrimitive* primitive2 = new MockPrimitive;
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1, intersects(_)).WillOnce(Return(true));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    ASSERT_TRUE(u.intersects(ray));
  }
  
  TEST(Union, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Union u;
    MockPrimitive* primitive1 = new MockPrimitive;
    MockPrimitive* primitive2 = new MockPrimitive;
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1, intersects(_)).WillOnce(Return(false));
    EXPECT_CALL(*primitive2, intersects(_)).WillOnce(Return(false));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    ASSERT_FALSE(u.intersects(ray));
  }
}
