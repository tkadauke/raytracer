#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "render/primitives/ClosedSolidUnion.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Union.h"
#include "render/materials/MatteMaterial.h"
#include "core/math/RayPacket.h"
#include "test/helpers/PrimitiveTestHelper.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace UnionTest {
  using namespace ::testing;
  using namespace render;
  using test::helpers::PacketStates4;
  using test::helpers::PacketStates8;

  TEST(Union, ShouldReturnClosestPrimitiveForUnion) {
    Union u;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1.get(), intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoint(HitPoint(primitive1.get(), 1.0, Vector3d(), Vector3d())),
                      Return(primitive1.get())));
    EXPECT_CALL(*primitive2.get(), intersect(_, _, _))
      .WillOnce(Return(static_cast<render::Primitive*>(nullptr)));

    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto result = u.intersect(ray, hitPoints, state);

    ASSERT_EQ(primitive1.get(), result);
  }

  TEST(Union, ShouldReturnSelfIfDifferenceHasMaterial) {
    Union u;
    u.setMaterial(std::make_shared<MatteMaterial>());

    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1.get(), intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoint(HitPoint(primitive1.get(), 1.0, Vector3d(), Vector3d())),
                      Return(primitive1.get())));
    EXPECT_CALL(*primitive2.get(), intersect(_, _, _))
      .WillOnce(Return(static_cast<render::Primitive*>(nullptr)));

    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto result = u.intersect(ray, hitPoints, state);

    ASSERT_EQ(&u, result);
  }

  TEST(Union, ShouldNotReturnAnyPrimitiveIfThereIsNoIntersection) {
    Union u;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1.get(), intersect(_, _, _))
      .WillOnce(Return(static_cast<render::Primitive*>(nullptr)));
    EXPECT_CALL(*primitive2.get(), intersect(_, _, _))
      .WillOnce(Return(static_cast<render::Primitive*>(nullptr)));

    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto result = u.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, result);
  }

  TEST(Union, ShouldBuildUnionOfHitPoints) {
    Union u;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1.get(), intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoint(HitPoint(primitive1.get(), 1.0, Vector3d(), Vector3d())),
                      Return(primitive1.get())));
    EXPECT_CALL(*primitive2.get(), intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoint(HitPoint(primitive2.get(), 5.0, Vector3d(), Vector3d())),
                      Return(primitive2.get())));

    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    u.intersect(ray, hitPoints, state);

    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(5, hitPoints.max().distance());
  }

  TEST(Union, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    Union u;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1, intersects(_, _)).WillOnce(Return(true));

    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));

    State state;
    ASSERT_TRUE(u.intersects(ray, state));
  }

  TEST(Union, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Union u;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    u.add(primitive1);
    u.add(primitive2);
    EXPECT_CALL(*primitive1.get(), intersects(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*primitive2.get(), intersects(_, _)).WillOnce(Return(false));

    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));

    State state;
    ASSERT_FALSE(u.intersects(ray, state));
  }

  TEST(Union, ShouldComposeRay4PacketIntervalsForPacketHits) {
    Union unionPrimitive;
    unionPrimitive.setMaterial(std::make_shared<MatteMaterial>());
    unionPrimitive.add(std::make_shared<Sphere>(Vector3d(), 1));
    unionPrimitive.add(std::make_shared<Sphere>(Vector3d(0, 0, 1), 1));
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 3), Vector3d(0, 0, 1)), Rayd(Vector3d(3, 0, -2), Vector3d(0, 0, 1))});
    PacketStates4 ps;

    const auto result = unionPrimitive.intersectPacketHits(rays, ps.states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&unionPrimitive, result.primitive(0));
    EXPECT_FALSE(result.scalarFallback(0));
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    EXPECT_FALSE(result.hit(3));
    for (const State& state : ps.lanes) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(Union, ShouldApplyMaterialOverrideToRay4PacketIntervals) {
    Union unionPrimitive;
    unionPrimitive.setMaterial(std::make_shared<MatteMaterial>());
    unionPrimitive.add(std::make_shared<Sphere>(Vector3d(), 1));
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 3), Vector3d(0, 0, 1)), Rayd(Vector3d(3, 0, -2), Vector3d(0, 0, 1))});
    PacketStates4 ps;

    const auto result = unionPrimitive.intersectPacketIntervals(rays, ps.states);

    ASSERT_TRUE(result.hit(0));
    ASSERT_TRUE(result.hasInterval(0));
    EXPECT_EQ(&unionPrimitive, result.primitive(0));
    EXPECT_EQ(&unionPrimitive, result.interval(0).min().primitive());
    EXPECT_EQ(&unionPrimitive, result.interval(0).max().primitive());
    EXPECT_FALSE(result.scalarFallback(0));
  }

  TEST(Union, ShouldComposeRay8PacketIntervalsForPacketHits) {
    Union unionPrimitive;
    unionPrimitive.setMaterial(std::make_shared<MatteMaterial>());
    unionPrimitive.add(std::make_shared<Sphere>(Vector3d(), 1));
    unionPrimitive.add(std::make_shared<Sphere>(Vector3d(0, 0, 1), 1));
    const Ray8 rays(std::array<Rayd, Ray8::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 3), Vector3d(0, 0, 1)), Rayd(Vector3d(3, 0, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, 0, -3), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, 3), Vector3d(0, 0, -1)),
      Rayd(Vector3d(0, 2, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0.25, -2), Vector3d(0, 0, 1))});
    PacketStates8 ps;

    const auto result = unionPrimitive.intersectPacketHits(rays, ps.states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&unionPrimitive, result.primitive(0));
    EXPECT_FALSE(result.scalarFallback(0));
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    EXPECT_FALSE(result.hit(3));
    ASSERT_TRUE(result.hit(4));
    EXPECT_EQ(&unionPrimitive, result.primitive(4));
    EXPECT_FALSE(result.scalarFallback(4));
    ASSERT_TRUE(result.hit(5));
    EXPECT_EQ(&unionPrimitive, result.primitive(5));
    EXPECT_FALSE(result.scalarFallback(5));
    EXPECT_FALSE(result.hit(6));
    ASSERT_TRUE(result.hit(7));
    EXPECT_EQ(&unionPrimitive, result.primitive(7));
    EXPECT_FALSE(result.scalarFallback(7));
    for (const State& state : ps.lanes) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(ClosedSolidUnion, ShouldComposeRay4PacketIntervalsForPacketHits) {
    ClosedSolidUnion unionPrimitive;
    unionPrimitive.setMaterial(std::make_shared<MatteMaterial>());
    unionPrimitive.add(std::make_shared<Sphere>(Vector3d(), 1));
    unionPrimitive.add(std::make_shared<Sphere>(Vector3d(3, 0, 0), 1));
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, 1)), Rayd(Vector3d(6, 0, -2), Vector3d(0, 0, 1))});
    PacketStates4 ps;

    const auto result = unionPrimitive.intersectPacketHits(rays, ps.states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&unionPrimitive, result.primitive(0));
    EXPECT_FALSE(result.scalarFallback(0));
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    EXPECT_FALSE(result.hit(3));
    for (const State& state : ps.lanes) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(ClosedSolidUnion, ShouldApplyMaterialOverrideToRay4PacketIntervals) {
    ClosedSolidUnion unionPrimitive;
    unionPrimitive.setMaterial(std::make_shared<MatteMaterial>());
    unionPrimitive.add(std::make_shared<Sphere>(Vector3d(), 1));
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 3), Vector3d(0, 0, 1)), Rayd(Vector3d(3, 0, -2), Vector3d(0, 0, 1))});
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    const auto result = unionPrimitive.intersectPacketIntervals(rays, states);

    ASSERT_TRUE(result.hit(0));
    ASSERT_TRUE(result.hasInterval(0));
    EXPECT_EQ(&unionPrimitive, result.primitive(0));
    EXPECT_EQ(&unionPrimitive, result.interval(0).min().primitive());
    EXPECT_EQ(&unionPrimitive, result.interval(0).max().primitive());
    EXPECT_FALSE(result.scalarFallback(0));
  }

  TEST(ClosedSolidUnion, ShouldComposeRay8PacketIntervalsForPacketHits) {
    ClosedSolidUnion unionPrimitive;
    unionPrimitive.setMaterial(std::make_shared<MatteMaterial>());
    unionPrimitive.add(std::make_shared<Sphere>(Vector3d(), 1));
    unionPrimitive.add(std::make_shared<Sphere>(Vector3d(3, 0, 0), 1));
    const Ray8 rays(std::array<Rayd, Ray8::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, 1)), Rayd(Vector3d(6, 0, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, 0, -3), Vector3d(0, 0, 1)), Rayd(Vector3d(3, 0, -3), Vector3d(0, 0, 1)),
      Rayd(Vector3d(3, 2, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(6, 0, 2), Vector3d(0, 0, -1))});
    std::array<State, Ray8::lanes> laneStates;
    PrimitivePacketState8 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3],
                                 &laneStates[4], &laneStates[5], &laneStates[6], &laneStates[7]};

    const auto result = unionPrimitive.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&unionPrimitive, result.primitive(0));
    EXPECT_FALSE(result.scalarFallback(0));
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    EXPECT_FALSE(result.hit(3));
    ASSERT_TRUE(result.hit(4));
    EXPECT_EQ(&unionPrimitive, result.primitive(4));
    EXPECT_FALSE(result.scalarFallback(4));
    ASSERT_TRUE(result.hit(5));
    EXPECT_EQ(&unionPrimitive, result.primitive(5));
    EXPECT_FALSE(result.scalarFallback(5));
    EXPECT_FALSE(result.hit(6));
    EXPECT_FALSE(result.hit(7));
    for (const State& state : laneStates) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }
}
