#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "raytracer/primitives/Intersection.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace IntersectionTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(Intersection, ShouldReturnSelfIfAllOfTheChildPrimitivesIntersect) {
    Intersection i;
    MockPrimitive* primitive1 = new MockPrimitive;
    MockPrimitive* primitive2 = new MockPrimitive;
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoints(HitPoint(1.0, Vector3d(), Vector3d()), HitPoint(4.0, Vector3d(), Vector3d())),
        Return(primitive1)
      )
    );
    EXPECT_CALL(*primitive2, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoints(HitPoint(2.0, Vector3d(), Vector3d()), HitPoint(5.0, Vector3d(), Vector3d())),
        Return(primitive2)
      )
    );
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Primitive* result = i.intersect(ray, hitPoints);
    
    ASSERT_EQ(&i, result);
  }
  
  TEST(Intersection, ShouldNotReturnAnyPrimitiveIfThereIsNoIntersection) {
    Intersection i;
    MockPrimitive* primitive1 = new MockPrimitive;
    MockPrimitive* primitive2 = new MockPrimitive;
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, intersect(_, _)).WillOnce(Return(static_cast<Primitive*>(0)));
    EXPECT_CALL(*primitive2, intersect(_, _)).WillOnce(Return(static_cast<Primitive*>(0)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Primitive* result = i.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
  }
  
  TEST(Intersection, ShouldNotReturnAnyPrimitiveIfNotAllChildrenIntersect) {
    Intersection i;
    MockPrimitive* primitive1 = new MockPrimitive;
    MockPrimitive* primitive2 = new MockPrimitive;
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoints(HitPoint(1.0, Vector3d(), Vector3d()), HitPoint(4.0, Vector3d(), Vector3d())),
        Return(primitive1)
      )
    );
    EXPECT_CALL(*primitive2, intersect(_, _)).WillOnce(Return(static_cast<Primitive*>(0)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Primitive* result = i.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
  }
  
  TEST(Intersection, ShouldNotReturnAnyPrimitiveIfNotAllChildrenIntersectInOverlappingIntervals) {
    Intersection i;
    MockPrimitive* primitive1 = new MockPrimitive;
    MockPrimitive* primitive2 = new MockPrimitive;
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoints(HitPoint(1.0, Vector3d(), Vector3d()), HitPoint(2.0, Vector3d(), Vector3d())),
        Return(primitive1)
      )
    );
    EXPECT_CALL(*primitive2, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoints(HitPoint(3.0, Vector3d(), Vector3d()), HitPoint(4.0, Vector3d(), Vector3d())),
        Return(primitive2)
      )
    );
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Primitive* result = i.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
  }
  
  TEST(Intersection, ShouldReturnTrueForIntersectsIfAllOfTheChildPrimitivesIntersect) {
    Intersection i;
    MockPrimitive* primitive1 = new MockPrimitive;
    MockPrimitive* primitive2 = new MockPrimitive;
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, intersects(_)).WillOnce(Return(true));
    EXPECT_CALL(*primitive2, intersects(_)).WillOnce(Return(true));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    bool result = i.intersects(ray);
    
    ASSERT_TRUE(result);
  }
  
  TEST(Intersection, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Intersection i;
    MockPrimitive* primitive1 = new MockPrimitive;
    MockPrimitive* primitive2 = new MockPrimitive;
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, intersects(_)).WillOnce(Return(false));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    bool result = i.intersects(ray);
    
    ASSERT_FALSE(result);
  }
  
  TEST(Intersection, ShouldReturnFalseForIntersectsIfNotAllChildrenIntersect) {
    Intersection i;
    MockPrimitive* primitive1 = new MockPrimitive;
    MockPrimitive* primitive2 = new MockPrimitive;
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, intersects(_)).WillOnce(Return(true));
    EXPECT_CALL(*primitive2, intersects(_)).WillOnce(Return(false));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    bool result = i.intersects(ray);
    
    ASSERT_FALSE(result);
  }
  
  TEST(Intersection, ShouldReturnBoundingBoxWithOneChild) {
    Intersection i;
    MockPrimitive* mockPrimitive = new MockPrimitive;
    i.add(mockPrimitive);
    
    BoundingBox bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    EXPECT_CALL(*mockPrimitive, boundingBox()).WillOnce(Return(bbox));
    
    ASSERT_EQ(bbox, i.boundingBox());
  }
  
  TEST(Intersection, ShouldReturnBoundingBoxWithMultipleChildren) {
    Intersection i;
    MockPrimitive* mockPrimitive1 = new MockPrimitive;
    MockPrimitive* mockPrimitive2 = new MockPrimitive;
    i.add(mockPrimitive1);
    i.add(mockPrimitive2);
    
    EXPECT_CALL(*mockPrimitive1, boundingBox()).WillOnce(Return(BoundingBox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1))));
    EXPECT_CALL(*mockPrimitive2, boundingBox()).WillOnce(Return(BoundingBox(Vector3d(0, 0, 0), Vector3d(2, 2, 2))));
    
    BoundingBox expected(Vector3d(0, 0, 0), Vector3d(1, 1, 1));
    ASSERT_EQ(expected, i.boundingBox());
  }
}