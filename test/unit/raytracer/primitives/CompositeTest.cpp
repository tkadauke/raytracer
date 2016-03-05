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
    auto mockPrimitive = std::make_shared<MockPrimitive>();
    composite.add(mockPrimitive);
    ASSERT_FALSE(composite.primitives().empty());
    ASSERT_EQ(mockPrimitive, composite.primitives().front());
  }
  
  TEST(Composite, ShouldDestructAllAddedPrimitives) {
    auto composite = std::make_shared<Composite>();
    auto mockPrimitive = std::make_shared<MockPrimitive>();
    composite->add(mockPrimitive);
    
    EXPECT_CALL(*mockPrimitive, destructorCall());
  }
  
  TEST(Composite, ShouldReturnIntersectedPrimitive) {
    Composite composite;
    auto primitive = std::make_shared<MockPrimitive>();
    composite.add(primitive);
    EXPECT_CALL(*primitive, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoint(HitPoint(1.0, Vector3d(), Vector3d())),
        Return(primitive.get())
      )
    );
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    auto result = composite.intersect(ray, hitPoints);
    
    ASSERT_EQ(primitive.get(), result);
  }
  
  TEST(Composite, ShouldNotReturnAnyPrimitiveIfThereIsNoIntersection) {
    Composite composite;
    auto primitive = std::make_shared<MockPrimitive>();
    composite.add(primitive);
    EXPECT_CALL(*primitive, intersect(_, _)).WillOnce(Return(static_cast<Primitive*>(0)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    auto result = composite.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
  }
  
  TEST(Composite, ShouldReturnClosestIntersectedPrimitiveIfThereIsMoreThanOneCandidate) {
    Composite composite;
    auto primitive1 = std::make_shared<MockPrimitive>();
    auto primitive2 = std::make_shared<MockPrimitive>();
    composite.add(primitive1);
    composite.add(primitive2);
    EXPECT_CALL(*primitive1, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoint(HitPoint(5.0, Vector3d(), Vector3d())),
        Return(primitive1.get())
      )
    );
    EXPECT_CALL(*primitive2, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoint(HitPoint(1.0, Vector3d(), Vector3d())),
        Return(primitive2.get())
      )
    );
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    auto result = composite.intersect(ray, hitPoints);
    
    ASSERT_EQ(primitive2.get(), result);
  }
  
  TEST(Composite, ShouldReturnTrueForIntersectsIfThereIsAnIntersection) {
    Composite composite;
    auto primitive1 = std::make_shared<MockPrimitive>();
    auto primitive2 = std::make_shared<MockPrimitive>();
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
    auto primitive1 = std::make_shared<MockPrimitive>();
    auto primitive2 = std::make_shared<MockPrimitive>();
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
    auto mockPrimitive = std::make_shared<MockPrimitive>();
    composite.add(mockPrimitive);
    
    BoundingBoxd bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    EXPECT_CALL(*mockPrimitive, boundingBox()).WillOnce(Return(bbox));
    
    ASSERT_EQ(bbox, composite.boundingBox());
  }
  
  TEST(Composite, ShouldReturnBoundingBoxWithMultipleChildren) {
    Composite composite;
    auto mockPrimitive1 = std::make_shared<MockPrimitive>();
    auto mockPrimitive2 = std::make_shared<MockPrimitive>();
    composite.add(mockPrimitive1);
    composite.add(mockPrimitive2);
    
    EXPECT_CALL(*mockPrimitive1, boundingBox()).WillOnce(
      Return(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1)))
    );
    EXPECT_CALL(*mockPrimitive2, boundingBox()).WillOnce(
      Return(BoundingBoxd(Vector3d(0, 0, 0), Vector3d(2, 2, 2)))
    );
    
    BoundingBoxd expected(Vector3d(-1, -1, -1), Vector3d(2, 2, 2));
    ASSERT_EQ(expected, composite.boundingBox());
  }
}
