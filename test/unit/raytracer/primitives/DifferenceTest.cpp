#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "raytracer/primitives/Difference.h"
#include "raytracer/materials/MatteMaterial.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace DifferenceTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(Difference, ShouldReturnClosestPrimitiveForDifference) {
    Difference i;
    auto primitive1 = std::make_shared<MockPrimitive>();
    auto primitive2 = std::make_shared<MockPrimitive>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoints(HitPoint(1.0, Vector3d(), Vector3d()), HitPoint(4.0, Vector3d(), Vector3d())),
        Return(primitive1.get())
      )
    );
    EXPECT_CALL(*primitive2, intersect(_, _)).WillOnce(Return(static_cast<Primitive*>(nullptr)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    auto result = i.intersect(ray, hitPoints);
    
    ASSERT_EQ(primitive1.get(), result);
  }

  TEST(Difference, ShouldReturnSelfIfDifferenceHasMaterial) {
    Difference i;
    i.setMaterial(new MatteMaterial);
    
    auto primitive1 = std::make_shared<MockPrimitive>();
    auto primitive2 = std::make_shared<MockPrimitive>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoints(HitPoint(1.0, Vector3d(), Vector3d()), HitPoint(4.0, Vector3d(), Vector3d())),
        Return(primitive1.get())
      )
    );
    EXPECT_CALL(*primitive2, intersect(_, _)).WillOnce(Return(static_cast<Primitive*>(nullptr)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    auto result = i.intersect(ray, hitPoints);
    
    ASSERT_EQ(&i, result);
  }
  
  TEST(Difference, ShouldNotReturnAnyPrimitiveIfFirstChildDoesNotIntersect) {
    Difference i;
    auto primitive1 = std::make_shared<MockPrimitive>();
    auto primitive2 = std::make_shared<MockPrimitive>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, intersect(_, _)).WillOnce(Return(static_cast<Primitive*>(nullptr)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    auto result = i.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
  }
}
