#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "render/State.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Primitive.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "test/helpers/PrimitiveTestHelper.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace PrimitiveTest {
  using namespace render;
  using namespace testing;
  using test::helpers::PacketStates4;

  TEST(Primitive, ShouldReturnTrueForIntersectsIfIntersectReturnsObject) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    ON_CALL(*primitive, intersects(_, _))
      .WillByDefault(Invoke(primitive.get(), &NiceMock<MockPrimitive>::defaultIntersects));
    EXPECT_CALL(*primitive, intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoint(HitPoint(primitive.get(), 1.0, Vector3d(), Vector3d(1, 0, 0))),
                      Return(primitive.get())));

    State state;
    ASSERT_TRUE(primitive->intersects(Rayd(Vector3d::null, Vector3d::one), state));
  }

  TEST(Primitive, ShouldReturnTrueForIntersectsIfIntersectReturnsNoObject) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    ON_CALL(*primitive, intersects(_, _))
      .WillByDefault(Invoke(primitive.get(), &NiceMock<MockPrimitive>::defaultIntersects));
    EXPECT_CALL(*primitive, intersect(_, _, _)).WillOnce(Return(nullptr));

    State state;
    ASSERT_FALSE(primitive->intersects(Rayd(Vector3d::null, Vector3d::one), state));
  }

  TEST(Primitive, ShouldIntersectRay4PacketWithScalarFallback) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    EXPECT_CALL(*primitive, intersect(_, _, _))
      .Times(4)
      .WillRepeatedly(
        DoAll(AddHitPoints(HitPoint(primitive.get(), 1.0, Vector3d(0, 0, 1), Vector3d(0, 0, -1)),
                           HitPoint(primitive.get(), 3.0, Vector3d(0, 0, 3), Vector3d(0, 0, 1))),
              Return(primitive.get())));

    const Ray4 rays(std::array<Rayf, 4>{
      Rayf(Vector3f(0, 0, 0), Vector3f(0, 0, 1)), Rayf(Vector3f(1, 0, 0), Vector3f(0, 0, 1)),
      Rayf(Vector3f(2, 0, 0), Vector3f(0, 0, 1)), Rayf(Vector3f(3, 0, 0), Vector3f(0, 0, 1))});

    State state;
    const auto result = primitive->Primitive::intersectPacket(rays, state);

    ASSERT_EQ(0b1111, result.hitMask);
    ASSERT_EQ(1.0f, result.tNear[0]);
    ASSERT_EQ(3.0f, result.tFar[3]);
  }

  TEST(Primitive, ShouldMaterializeRay4PacketHitsWithScalarFallback) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    EXPECT_CALL(*primitive, intersect(_, _, _))
      .Times(4)
      .WillRepeatedly(
        DoAll(AddHitPoints(HitPoint(primitive.get(), 1.0, Vector3d(0, 0, 1), Vector3d(0, 0, -1)),
                           HitPoint(primitive.get(), 3.0, Vector3d(0, 0, 3), Vector3d(0, 0, 1))),
              Return(primitive.get())));

    const Ray4 rays(std::array<Rayf, 4>{
      Rayf(Vector3f(0, 0, 0), Vector3f(0, 0, 1)), Rayf(Vector3f(1, 0, 0), Vector3f(0, 0, 1)),
      Rayf(Vector3f(2, 0, 0), Vector3f(0, 0, 1)), Rayf(Vector3f(3, 0, 0), Vector3f(0, 0, 1))});

    PacketStates4 ps;
    const auto result = primitive->Primitive::intersectPacketHits(rays, ps.states);

    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      ASSERT_TRUE(result.hit(lane)) << "lane " << lane;
      ASSERT_EQ(primitive.get(), result.primitive(lane));
      ASSERT_EQ(1.0, result.hitPoint(lane).distance());
      EXPECT_TRUE(result.scalarFallback(lane));
      EXPECT_EQ(1u, ps.lanes[lane].packetHitScalarFallbacks);
    }
  }

  TEST(Primitive, ShouldSkipInactiveRay4PacketHitScalarFallbackLanes) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    EXPECT_CALL(*primitive, intersect(_, _, _))
      .Times(2)
      .WillRepeatedly(
        DoAll(AddHitPoints(HitPoint(primitive.get(), 1.0, Vector3d(0, 0, 1), Vector3d(0, 0, -1)),
                           HitPoint(primitive.get(), 3.0, Vector3d(0, 0, 3), Vector3d(0, 0, 1))),
              Return(primitive.get())));

    const Ray4 rays(std::array<Rayf, 4>{
      Rayf(Vector3f(0, 0, 0), Vector3f(0, 0, 1)), Rayf(Vector3f(1, 0, 0), Vector3f(0, 0, 1)),
      Rayf(Vector3f(2, 0, 0), Vector3f(0, 0, 1)), Rayf(Vector3f(3, 0, 0), Vector3f(0, 0, 1))});

    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], nullptr, &laneStates[2], nullptr};
    const auto result = primitive->Primitive::intersectPacketHits(rays, states);

    EXPECT_TRUE(result.hit(0));
    EXPECT_FALSE(result.hit(1));
    EXPECT_TRUE(result.hit(2));
    EXPECT_FALSE(result.hit(3));
    EXPECT_EQ(1u, laneStates[0].packetHitScalarFallbacks);
    EXPECT_EQ(0u, laneStates[1].packetHitScalarFallbacks);
    EXPECT_EQ(1u, laneStates[2].packetHitScalarFallbacks);
    EXPECT_EQ(0u, laneStates[3].packetHitScalarFallbacks);
  }

  TEST(Primitive, ShouldMaterializeRay4PacketIntervalsWithScalarFallback) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    EXPECT_CALL(*primitive, intersect(_, _, _))
      .Times(4)
      .WillRepeatedly(
        DoAll(AddHitPoints(HitPoint(primitive.get(), 1.0, Vector3d(0, 0, 1), Vector3d(0, 0, -1)),
                           HitPoint(primitive.get(), 3.0, Vector3d(0, 0, 3), Vector3d(0, 0, 1))),
              Return(primitive.get())));

    const Ray4 rays(std::array<Rayf, 4>{
      Rayf(Vector3f(0, 0, 0), Vector3f(0, 0, 1)), Rayf(Vector3f(1, 0, 0), Vector3f(0, 0, 1)),
      Rayf(Vector3f(2, 0, 0), Vector3f(0, 0, 1)), Rayf(Vector3f(3, 0, 0), Vector3f(0, 0, 1))});

    PacketStates4 ps;
    const auto result = primitive->Primitive::intersectPacketIntervals(rays, ps.states);

    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      ASSERT_TRUE(result.hit(lane)) << "lane " << lane;
      ASSERT_TRUE(result.hasInterval(lane)) << "lane " << lane;
      ASSERT_EQ(primitive.get(), result.primitive(lane));
      ASSERT_EQ(1.0, result.interval(lane).min().distance());
      ASSERT_EQ(3.0, result.interval(lane).max().distance());
      EXPECT_TRUE(result.scalarFallback(lane));
      EXPECT_EQ(1u, ps.lanes[lane].packetHitScalarFallbacks);
    }
  }

  TEST(Primitive, ShouldSkipInactiveRay4PacketIntervalScalarFallbackLanes) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    EXPECT_CALL(*primitive, intersect(_, _, _))
      .Times(2)
      .WillRepeatedly(
        DoAll(AddHitPoints(HitPoint(primitive.get(), 1.0, Vector3d(0, 0, 1), Vector3d(0, 0, -1)),
                           HitPoint(primitive.get(), 3.0, Vector3d(0, 0, 3), Vector3d(0, 0, 1))),
              Return(primitive.get())));

    const Ray4 rays(std::array<Rayf, 4>{
      Rayf(Vector3f(0, 0, 0), Vector3f(0, 0, 1)), Rayf(Vector3f(1, 0, 0), Vector3f(0, 0, 1)),
      Rayf(Vector3f(2, 0, 0), Vector3f(0, 0, 1)), Rayf(Vector3f(3, 0, 0), Vector3f(0, 0, 1))});

    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], nullptr, &laneStates[2], nullptr};
    const auto result = primitive->Primitive::intersectPacketIntervals(rays, states);

    EXPECT_TRUE(result.hasInterval(0));
    EXPECT_FALSE(result.hasInterval(1));
    EXPECT_TRUE(result.hasInterval(2));
    EXPECT_FALSE(result.hasInterval(3));
    EXPECT_EQ(1u, laneStates[0].packetHitScalarFallbacks);
    EXPECT_EQ(0u, laneStates[1].packetHitScalarFallbacks);
    EXPECT_EQ(1u, laneStates[2].packetHitScalarFallbacks);
    EXPECT_EQ(0u, laneStates[3].packetHitScalarFallbacks);
  }

  TEST(Primitive, ShouldReturnFarthestPoint) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    ON_CALL(*primitive, farthestPoint(_))
      .WillByDefault(Invoke(primitive.get(), &NiceMock<MockPrimitive>::defaultFarthestPoint));

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
