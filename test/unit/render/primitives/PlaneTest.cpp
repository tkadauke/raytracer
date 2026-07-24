#include <gtest/gtest.h>
#include "render/State.h"
#include "render/primitives/Plane.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include "test/helpers/PrimitiveTestHelper.h"

namespace PlaneTest {
  using namespace render;
  using test::helpers::PacketStates4;
  using test::helpers::PacketStates8;

  TEST(Plane, ShouldInitializeWithValues) {
    Plane plane(Vector3d(0, 1, 0), 0);
  }

  TEST(Plane, ShouldIntersectWithRay) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Rayd ray(Vector3d(0, 1, 0), Vector3d(0, -1, 0));

    State state;
    HitPointInterval hitPoints;
    auto primitive = plane.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &plane);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 1, 0), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(Plane, ShouldNotIntersectWithParallelRay) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto primitive = plane.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Plane, ShouldNotIntersectIfPointIsBehindRayOrigin) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Rayd ray(Vector3d(0, -1, 0), Vector3d(0, -1, 0));

    State state;
    HitPointInterval hitPoints;
    auto primitive = plane.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Plane, ShouldIntersectRay4PacketLikeScalarRays) {
    Plane plane(Vector3d(0, 1, 0), 0);
    const std::array<Rayf, 4> rayArray{
      Rayf(Vector3f(0, 1, 0), Vector3f(0, -1, 0)), Rayf(Vector3f(0, 1, 0), Vector3f(1, 0, 0)),
      Rayf(Vector3f(0, -1, 0), Vector3f(0, -1, 0)), Rayf(Vector3f(0, 2, 0), Vector3f(0, -2, 0))};

    State packetState;
    const auto result = plane.intersectPacket(Ray4(rayArray), packetState);

    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      State scalarState;
      HitPointInterval hitPoints;
      const auto primitive = plane.intersect(Rayd(rayArray[lane]), hitPoints, scalarState);
      ASSERT_EQ(primitive != nullptr, result.hit(lane)) << "lane " << lane;
      if (primitive != nullptr) {
        ASSERT_NEAR(hitPoints.min().distance(), result.tNear[lane], 1e-5) << "lane " << lane;
      }
    }
    ASSERT_EQ(2, packetState.intersectionHits);
    ASSERT_EQ(2, packetState.intersectionMisses);
  }

  TEST(Plane, ShouldMaterializeRay4PacketHits) {
    Plane plane(Vector3d(0, 1, 0), 0);
    const std::array<Rayd, 4> rayArray{
      Rayd(Vector3d(0, 1, 0), Vector3d(0, -1, 0)), Rayd(Vector3d(0, 1, 0), Vector3d(1, 0, 0)),
      Rayd(Vector3d(0, -1, 0), Vector3d(0, -1, 0)), Rayd(Vector3d(0, 2, 0), Vector3d(0, -2, 0))};
    PacketStates4 ps;

    const auto result = plane.intersectPacketHits(Ray4(rayArray), ps.states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&plane, result.primitive(0));
    EXPECT_EQ(Vector3d(0, 0, 0), result.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 1, 0), result.hitPoint(0).normal());
    EXPECT_EQ(1, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    ASSERT_TRUE(result.hit(3));
    EXPECT_EQ(Vector3d(0, 0, 0), result.hitPoint(3).point());
    EXPECT_EQ(Vector3d(0, 1, 0), result.hitPoint(3).normal());
    EXPECT_EQ(1, result.hitPoint(3).distance());
    EXPECT_EQ(1, ps.lanes[0].intersectionHits);
    EXPECT_EQ(1, ps.lanes[1].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[2].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[3].intersectionHits);
  }

  TEST(Plane, ShouldMaterializeRay8PacketHits) {
    Plane plane(Vector3d(0, 1, 0), 0);
    const std::array<Rayd, 8> rayArray{
      Rayd(Vector3d(0, 1, 0), Vector3d(0, -1, 0)),  Rayd(Vector3d(0, 1, 0), Vector3d(1, 0, 0)),
      Rayd(Vector3d(0, -1, 0), Vector3d(0, -1, 0)), Rayd(Vector3d(0, 2, 0), Vector3d(0, -2, 0)),
      Rayd(Vector3d(0, 3, 0), Vector3d(0, -3, 0)),  Rayd(Vector3d(0, 3, 0), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, -3, 0), Vector3d(0, 1, 0)),  Rayd(Vector3d(0, 4, 0), Vector3d(0, -4, 0))};
    PacketStates8 ps;

    const auto result = plane.intersectPacketHits(Ray8(rayArray), ps.states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&plane, result.primitive(0));
    EXPECT_EQ(Vector3d(0, 0, 0), result.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 1, 0), result.hitPoint(0).normal());
    EXPECT_EQ(1, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    ASSERT_TRUE(result.hit(3));
    EXPECT_EQ(Vector3d(0, 0, 0), result.hitPoint(3).point());
    EXPECT_EQ(Vector3d(0, 1, 0), result.hitPoint(3).normal());
    EXPECT_EQ(1, result.hitPoint(3).distance());
    ASSERT_TRUE(result.hit(4));
    EXPECT_EQ(1, result.hitPoint(4).distance());
    EXPECT_FALSE(result.hit(5));
    ASSERT_TRUE(result.hit(6));
    EXPECT_EQ(3, result.hitPoint(6).distance());
    ASSERT_TRUE(result.hit(7));
    EXPECT_EQ(1, result.hitPoint(7).distance());
    EXPECT_EQ(1, ps.lanes[0].intersectionHits);
    EXPECT_EQ(1, ps.lanes[1].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[2].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[3].intersectionHits);
    EXPECT_EQ(1, ps.lanes[4].intersectionHits);
    EXPECT_EQ(1, ps.lanes[5].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[6].intersectionHits);
    EXPECT_EQ(1, ps.lanes[7].intersectionHits);
    for (const auto& state : ps.lanes) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(Plane, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Rayd ray(Vector3d(0, 1, 0), Vector3d(0, -1, 0));

    State state;
    ASSERT_TRUE(plane.intersects(ray, state));
  }

  TEST(Plane, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Rayd ray(Vector3d(0, -1, 0), Vector3d(0, -1, 0));

    State state;
    ASSERT_FALSE(plane.intersects(ray, state));
  }

  TEST(Plane, ShouldReturnBoundingBox) {
    // TODO
  }
}
