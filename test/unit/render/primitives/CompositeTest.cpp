#include <gtest/gtest.h>
#include "render/State.h"
#include "core/math/RayPacket.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Sphere.h"
#include "test/mocks/raytracer/MockPrimitive.h"

#include <vector>

namespace CompositeTest {
  using namespace ::testing;
  using namespace render;

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
    EXPECT_CALL(*primitive, intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoint(HitPoint(primitive.get(), 1.0, Vector3d(), Vector3d())),
                      Return(primitive.get())));

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
    EXPECT_CALL(*primitive, intersect(_, _, _))
      .WillOnce(Return(static_cast<render::Primitive*>(0)));

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
    EXPECT_CALL(*primitive1, intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoint(HitPoint(primitive1.get(), 5.0, Vector3d(), Vector3d())),
                      Return(primitive1.get())));
    EXPECT_CALL(*primitive2, intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoint(HitPoint(primitive2.get(), 1.0, Vector3d(), Vector3d())),
                      Return(primitive2.get())));

    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto result = composite.intersect(ray, hitPoints, state);

    ASSERT_EQ(primitive2.get(), result);
  }

  TEST(Composite, ShouldMaterializeRay4PacketHitsWithClosestPrimitivePerLane) {
    Composite composite;
    auto near = std::make_shared<Sphere>(Vector3d(0, 0, -2), 0.5);
    auto far = std::make_shared<Sphere>(Vector3d(0, 0, 2), 0.5);
    composite.add(far);
    composite.add(near);

    const Ray4 rays(std::array<Rayd, 4>{
      Rayd(Vector3d(0, 0, -10), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 100, -10), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, 0, 10), Vector3d(0, 0, -1)), Rayd(Vector3d(0, 0, -10), Vector3d(0, 1, 0))});

    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};
    const auto result = composite.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(near.get(), result.primitive(0));
    EXPECT_NEAR(7.5, result.hitPoint(0).distance(), 1e-6);
    ASSERT_FALSE(result.hit(1));
    ASSERT_TRUE(result.hit(2));
    EXPECT_EQ(far.get(), result.primitive(2));
    EXPECT_NEAR(7.5, result.hitPoint(2).distance(), 1e-6);
    ASSERT_FALSE(result.hit(3));
  }

  TEST(Composite, ShouldMaskInactivePacketStateLanesBeforeChildMaterialization) {
    Composite composite;
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    composite.add(primitive);
    ON_CALL(*primitive, calculateBoundingBox())
      .WillByDefault(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    ON_CALL(*primitive, intersect(_, _, _)).WillByDefault(Return(nullptr));

    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 3, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, -1)), Rayd(Vector3d(3, 0, -2), Vector3d(0, 0, 1))});
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    composite.intersectPacketHits(rays, states);

    EXPECT_EQ(1u, laneStates[0].packetHitScalarFallbacks);
    EXPECT_EQ(0u, laneStates[1].packetHitScalarFallbacks);
    EXPECT_EQ(1u, laneStates[2].packetHitScalarFallbacks);
    EXPECT_EQ(0u, laneStates[3].packetHitScalarFallbacks);
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

    EXPECT_CALL(*mockPrimitive1, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1))));
    EXPECT_CALL(*mockPrimitive2, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(Vector3d(0, 0, 0), Vector3d(2, 2, 2))));

    BoundingBoxd expected(Vector3d(-1, -1, -1), Vector3d(2, 2, 2));
    ASSERT_EQ(expected, composite.boundingBox());
  }

  TEST(Composite, ShouldVisitNestedLeavesWithInheritedMaterial) {
    Composite root;
    auto inherited = std::make_shared<MatteMaterial>();
    root.setMaterial(inherited);

    auto child = std::make_shared<Composite>();
    auto leaf = std::make_shared<NiceMock<MockPrimitive>>();
    child->add(leaf);
    root.add(child);

    std::vector<const Primitive*> visited;
    std::vector<std::shared_ptr<Material>> materials;
    root.forEachLeaf([&](const Primitive* primitive, std::shared_ptr<Material> material) {
      visited.push_back(primitive);
      materials.push_back(material);
    });

    ASSERT_THAT(visited, ElementsAre(leaf.get()));
    ASSERT_THAT(materials, ElementsAre(inherited));
  }

  TEST(Composite, ShouldPreferLeafMaterialOverInheritedMaterial) {
    Composite composite;
    auto inherited = std::make_shared<MatteMaterial>();
    auto own = std::make_shared<MatteMaterial>();
    composite.setMaterial(inherited);

    auto leaf = std::make_shared<NiceMock<MockPrimitive>>();
    leaf->setMaterial(own);
    composite.add(leaf);

    std::shared_ptr<Material> visitedMaterial;
    composite.forEachLeaf(
      [&](const Primitive*, std::shared_ptr<Material> material) { visitedMaterial = material; });

    ASSERT_EQ(own, visitedMaterial);
  }
}
