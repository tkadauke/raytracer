#include "gtest.h"
#include "raytracer/State.h"
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
    auto mockPrimitive = std::make_shared<NiceMock<MockPrimitive>>();
    composite.add(mockPrimitive);
    ASSERT_FALSE(composite.primitives().empty());
    ASSERT_EQ(mockPrimitive, composite.primitives().front());
  }
  
  TEST(Composite, ShouldDestructAllAddedPrimitives) {
    auto composite = std::make_shared<Composite>();
    auto mockPrimitive = std::make_shared<NiceMock<MockPrimitive>>();
    composite->add(mockPrimitive);
    
    mockPrimitive->expectDestructorCall();
  }
  
  TEST(Composite, ShouldReturnIntersectedPrimitive) {
    Composite composite;
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    composite.add(primitive);
    EXPECT_CALL(*primitive, intersect(_, _, _)).WillOnce(
      DoAll(
        AddHitPoint(HitPoint(primitive.get(), 1.0, Vector3d(), Vector3d())),
        Return(primitive.get())
      )
    );
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto result = composite.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(primitive.get(), result);
  }
  
  TEST(Composite, ShouldNotReturnAnyPrimitiveIfThereIsNoIntersection) {
    Composite composite;
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    composite.add(primitive);
    EXPECT_CALL(*primitive, intersect(_, _, _)).WillOnce(Return(static_cast<Primitive*>(0)));
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto result = composite.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, result);
  }
  
  TEST(Composite, ShouldReturnClosestIntersectedPrimitiveIfThereIsMoreThanOneCandidate) {
    Composite composite;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    composite.add(primitive1);
    composite.add(primitive2);
    EXPECT_CALL(*primitive1, intersect(_, _, _)).WillOnce(
      DoAll(
        AddHitPoint(HitPoint(primitive1.get(), 5.0, Vector3d(), Vector3d())),
        Return(primitive1.get())
      )
    );
    EXPECT_CALL(*primitive2, intersect(_, _, _)).WillOnce(
      DoAll(
        AddHitPoint(HitPoint(primitive2.get(), 1.0, Vector3d(), Vector3d())),
        Return(primitive2.get())
      )
    );
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto result = composite.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(primitive2.get(), result);
  }
  
  TEST(Composite, ShouldReturnTrueForIntersectsIfThereIsAnIntersection) {
    Composite composite;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    composite.add(primitive1);
    composite.add(primitive2);
    EXPECT_CALL(*primitive1, intersects(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*primitive2, intersects(_, _)).WillOnce(Return(true));
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    bool result = composite.intersects(ray, state);
    
    ASSERT_TRUE(result);
  }
  
  TEST(Composite, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Composite composite;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    composite.add(primitive1);
    composite.add(primitive2);
    EXPECT_CALL(*primitive1, intersects(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*primitive2, intersects(_, _)).WillOnce(Return(false));
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    bool result = composite.intersects(ray, state);
    
    ASSERT_FALSE(result);
  }

  TEST(Composite, ShouldReturnBoundingBoxWithOneChild) {
    Composite composite;
    auto mockPrimitive = std::make_shared<NiceMock<MockPrimitive>>();
    composite.add(mockPrimitive);
    
    BoundingBoxd bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    EXPECT_CALL(*mockPrimitive, calculateBoundingBox()).WillOnce(Return(bbox));
    
    ASSERT_EQ(bbox, composite.boundingBox());
  }
  
  TEST(Composite, ShouldReturnBoundingBoxWithMultipleChildren) {
    Composite composite;
    auto mockPrimitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto mockPrimitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    composite.add(mockPrimitive1);
    composite.add(mockPrimitive2);
    
    EXPECT_CALL(*mockPrimitive1, calculateBoundingBox()).WillOnce(
      Return(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1)))
    );
    EXPECT_CALL(*mockPrimitive2, calculateBoundingBox()).WillOnce(
      Return(BoundingBoxd(Vector3d(0, 0, 0), Vector3d(2, 2, 2)))
    );
    
    BoundingBoxd expected(Vector3d(-1, -1, -1), Vector3d(2, 2, 2));
    ASSERT_EQ(expected, composite.boundingBox());
  }
}
