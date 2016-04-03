#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "raytracer/State.h"
#include "raytracer/primitives/Difference.h"
#include "raytracer/materials/MatteMaterial.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace DifferenceTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(Difference, ShouldReturnClosestPrimitiveForDifference) {
    Difference i;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox()).WillOnce(Return(BoundingBoxd(-Vector3d::one(), Vector3d::one())));
    EXPECT_CALL(*primitive1, intersect(_, _, _)).WillOnce(
      DoAll(
        AddHitPoints(
          HitPoint(primitive1.get(), 1.0, Vector3d(), Vector3d()),
          HitPoint(primitive1.get(), 4.0, Vector3d(), Vector3d())
        ),
        Return(primitive1.get())
      )
    );
    EXPECT_CALL(*primitive2, intersect(_, _, _)).WillOnce(Return(static_cast<Primitive*>(nullptr)));
    
    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto result = i.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(primitive1.get(), result);
  }

  TEST(Difference, ShouldReturnSelfIfDifferenceHasMaterial) {
    Difference i;
    i.setMaterial(new MatteMaterial);
    
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox()).WillOnce(Return(BoundingBoxd(-Vector3d::one(), Vector3d::one())));
    EXPECT_CALL(*primitive1, intersect(_, _, _)).WillOnce(
      DoAll(
        AddHitPoints(
          HitPoint(primitive1.get(), 1.0, Vector3d(), Vector3d()),
          HitPoint(primitive1.get(), 4.0, Vector3d(), Vector3d())
        ),
        Return(primitive1.get())
      )
    );
    EXPECT_CALL(*primitive2, intersect(_, _, _)).WillOnce(Return(static_cast<Primitive*>(nullptr)));
    
    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto result = i.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(&i, result);
  }

  TEST(Difference, ShouldNotReturnAnyPrimitiveRayOutsideBoundingBox) {
    Difference i;
    i.setMaterial(new MatteMaterial);
    
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox()).WillOnce(Return(BoundingBoxd::undefined()));
    
    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto result = i.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, result);
  }

  TEST(Difference, ShouldNotReturnAnyPrimitiveIfFirstChildDoesNotIntersect) {
    Difference i;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox()).WillOnce(Return(BoundingBoxd(-Vector3d::one(), Vector3d::one())));
    EXPECT_CALL(*primitive1, intersect(_, _, _)).WillOnce(Return(static_cast<Primitive*>(nullptr)));
    
    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto result = i.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, result);
  }

  TEST(Difference, ShouldReturnBoundingBoxWithOneChild) {
    Difference i;
    auto mockPrimitive = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(mockPrimitive);
    
    BoundingBoxd bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    EXPECT_CALL(*mockPrimitive, calculateBoundingBox()).WillOnce(Return(bbox));
    
    ASSERT_EQ(bbox, i.boundingBox());
  }
  
  TEST(Difference, ShouldReturnBoundingBoxOfFirstChildIfThereAreMultipleChildren) {
    Difference i;
    auto mockPrimitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto mockPrimitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(mockPrimitive1);
    i.add(mockPrimitive2);
    
    EXPECT_CALL(*mockPrimitive1, calculateBoundingBox()).WillOnce(Return(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1))));
    EXPECT_CALL(*mockPrimitive2, calculateBoundingBox()).Times(0);
    
    BoundingBoxd expected(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    ASSERT_EQ(expected, i.boundingBox());
  }
}
