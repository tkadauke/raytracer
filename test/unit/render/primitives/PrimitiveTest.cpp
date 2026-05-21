#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "render/State.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Primitive.h"
#include "core/math/Ray.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace PrimitiveTest {
  using namespace render;
using namespace render;
using namespace render;
  using namespace testing;

  TEST(Primitive, ShouldReturnTrueForIntersectsIfIntersectReturnsObject) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    ON_CALL(*primitive, intersects(_, _)).WillByDefault(Invoke(primitive.get(), &NiceMock<MockPrimitive>::defaultIntersects));
    EXPECT_CALL(*primitive, intersect(_, _, _)).WillOnce(
      DoAll(
        AddHitPoint(HitPoint(primitive.get(), 1.0, Vector3d(), Vector3d(1, 0, 0))),
        Return(primitive.get())
      )
    );
    
    State state;
    ASSERT_TRUE(primitive->intersects(Rayd(Vector3d::null, Vector3d::one), state));
  }

  TEST(Primitive, ShouldReturnTrueForIntersectsIfIntersectReturnsNoObject) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    ON_CALL(*primitive, intersects(_, _)).WillByDefault(Invoke(primitive.get(), &NiceMock<MockPrimitive>::defaultIntersects));
    EXPECT_CALL(*primitive, intersect(_, _, _)).WillOnce(Return(nullptr));
    
    State state;
    ASSERT_FALSE(primitive->intersects(Rayd(Vector3d::null, Vector3d::one), state));
  }
  
  TEST(Primitive, ShouldReturnFarthestPoint) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    ON_CALL(*primitive, farthestPoint(_)).WillByDefault(Invoke(primitive.get(), &NiceMock<MockPrimitive>::defaultFarthestPoint));
    
    ASSERT_TRUE(primitive->farthestPoint(Vector3d::up()).isUndefined());
  }

  TEST(Primitive, ShouldVisitItselfAsLeafWithOwnMaterial) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    auto material = std::make_shared<MatteMaterial>();
    primitive->setMaterial(material);

    const Primitive* visited = nullptr;
    std::shared_ptr<Material> visitedMaterial;
    primitive->forEachLeaf([&](const Primitive* leaf, std::shared_ptr<Material> material) {
      visited = leaf;
      visitedMaterial = material;
    });

    ASSERT_EQ(primitive.get(), visited);
    ASSERT_EQ(material, visitedMaterial);
  }

  TEST(Primitive, ShouldVisitItselfAsLeafWithInheritedMaterial) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    auto inherited = std::make_shared<MatteMaterial>();

    std::shared_ptr<Material> visitedMaterial;
    primitive->forEachLeaf(inherited, [&](const Primitive*, std::shared_ptr<Material> material) {
      visitedMaterial = material;
    });

    ASSERT_EQ(inherited, visitedMaterial);
  }
}
