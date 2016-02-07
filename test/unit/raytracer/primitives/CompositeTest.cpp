#include "gtest.h"
#include "raytracer/primitives/Composite.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace CompositeTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(Composite, ShouldInitializeWithEmptyList) {
    Composite composite;
    ASSERT_TRUE(composite.primitives().empty());
  }
  
  TEST(Composite, ShouldAddPrimitive) {
    Composite composite;
    auto mockPrimitive = new MockPrimitive;
    composite.add(mockPrimitive);
    ASSERT_FALSE(composite.primitives().empty());
    ASSERT_EQ(mockPrimitive, composite.primitives().front());
  }
  
  TEST(Composite, ShouldDestructAllAddedPrimitives) {
    auto composite = new Composite;
    auto mockPrimitive = new MockPrimitive;
    composite->add(mockPrimitive);
    
    EXPECT_CALL(*mockPrimitive, destructorCall());
    
    delete composite;
  }
  
  TEST(Composite, ShouldSetParent) {
    Composite composite;
    auto mockPrimitive = new MockPrimitive;
    composite.add(mockPrimitive);
    
    ASSERT_EQ(&composite, mockPrimitive->parent());
  }
  
  TEST(Composite, ShouldReturnIntersectedPrimitive) {
    Composite composite;
    auto primitive = new MockPrimitive;
    composite.add(primitive);
    EXPECT_CALL(*primitive, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(1.0, Vector3d(), Vector3d())), Return(primitive)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    auto result = composite.intersect(ray, hitPoints);
    
    ASSERT_EQ(primitive, result);
  }
  
  TEST(Composite, ShouldNotReturnAnyPrimitiveIfThereIsNoIntersection) {
    Composite composite;
    auto primitive = new MockPrimitive;
    composite.add(primitive);
    EXPECT_CALL(*primitive, intersect(_, _)).WillOnce(Return(static_cast<Primitive*>(0)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    auto result = composite.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
  }
  
  TEST(Composite, ShouldReturnClosestIntersectedPrimitiveIfThereIsMoreThanOneCandidate) {
    Composite composite;
    auto primitive1 = new MockPrimitive;
    auto primitive2 = new MockPrimitive;
    composite.add(primitive1);
    composite.add(primitive2);
    EXPECT_CALL(*primitive1, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(5.0, Vector3d(), Vector3d())), Return(primitive1)));
    EXPECT_CALL(*primitive2, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(1.0, Vector3d(), Vector3d())), Return(primitive2)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    auto result = composite.intersect(ray, hitPoints);
    
    ASSERT_EQ(primitive2, result);
  }
  
  TEST(Composite, ShouldReturnTrueForIntersectsIfThereIsAnIntersection) {
    Composite composite;
    auto primitive1 = new MockPrimitive;
    auto primitive2 = new MockPrimitive;
    composite.add(primitive1);
    composite.add(primitive2);
    EXPECT_CALL(*primitive1, intersects(_)).WillOnce(Return(false));
    EXPECT_CALL(*primitive2, intersects(_)).WillOnce(Return(true));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    bool result = composite.intersects(ray);
    
    ASSERT_TRUE(result);
  }
  
  TEST(Composite, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Composite composite;
    auto primitive1 = new MockPrimitive;
    auto primitive2 = new MockPrimitive;
    composite.add(primitive1);
    composite.add(primitive2);
    EXPECT_CALL(*primitive1, intersects(_)).WillOnce(Return(false));
    EXPECT_CALL(*primitive2, intersects(_)).WillOnce(Return(false));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    bool result = composite.intersects(ray);
    
    ASSERT_FALSE(result);
  }

  TEST(Composite, ShouldReturnBoundingBoxWithOneChild) {
    Composite composite;
    auto mockPrimitive = new MockPrimitive;
    composite.add(mockPrimitive);
    
    BoundingBox bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    EXPECT_CALL(*mockPrimitive, boundingBox()).WillOnce(Return(bbox));
    
    ASSERT_EQ(bbox, composite.boundingBox());
  }
  
  TEST(Composite, ShouldReturnBoundingBoxWithMultipleChildren) {
    Composite composite;
    auto mockPrimitive1 = new MockPrimitive;
    auto mockPrimitive2 = new MockPrimitive;
    composite.add(mockPrimitive1);
    composite.add(mockPrimitive2);
    
    EXPECT_CALL(*mockPrimitive1, boundingBox()).WillOnce(Return(BoundingBox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1))));
    EXPECT_CALL(*mockPrimitive2, boundingBox()).WillOnce(Return(BoundingBox(Vector3d(0, 0, 0), Vector3d(2, 2, 2))));
    
    BoundingBox expected(Vector3d(-1, -1, -1), Vector3d(2, 2, 2));
    ASSERT_EQ(expected, composite.boundingBox());
  }
}
