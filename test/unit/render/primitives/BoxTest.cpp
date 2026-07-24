#include "gtest/gtest.h"
#include "render/State.h"
#include "render/primitives/Box.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include "test/helpers/PrimitiveTestHelper.h"

namespace BoxTest {
  using namespace render;
  using test::helpers::PacketStates4;
  using test::helpers::PacketStates8;

  TEST(Box, ShouldInitializeWithValues) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
  }

  TEST(Box, ShouldIntersectWithRayInXDirection) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Rayd ray(Vector3d(-2, 0, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto primitive = box.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &box);
    ASSERT_EQ(Vector3d(-1, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(-1, 0, 0), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(Box, ShouldIntersectWithRayInYDirection) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Rayd ray(Vector3d(0, -2, 0), Vector3d(0, 1, 0));

    State state;
    HitPointInterval hitPoints;
    auto primitive = box.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &box);
    ASSERT_EQ(Vector3d(0, -1, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, -1, 0), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(Box, ShouldIntersectWithRayInZDirection) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = box.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &box);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(Box, ShouldReportFaceTextureCoordinates) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Rayd ray(Vector3d(0.25, 0.5, -2), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = box.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &box);
    EXPECT_EQ(Vector3d(0.25, 0.5, -1), hitPoints.min().point());
    EXPECT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    EXPECT_EQ(Vector2d(0.625, 0.25), hitPoints.min().uv());
    EXPECT_EQ(Vector3d(0.25, 0.5, 1), hitPoints.max().point());
    EXPECT_EQ(Vector3d(0, 0, 1), hitPoints.max().normal());
    EXPECT_EQ(Vector2d(0.625, 0.75), hitPoints.max().uv());
  }

  TEST(Box, ShouldIntersectIfRayIsTangentToPrimitive) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Rayd ray(Vector3d(0, 1, -2), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = box.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &box);
    ASSERT_EQ(Vector3d(0, 1, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(Box, ShouldNotIntersectWithMissingRay) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 1, 0));

    State state;
    HitPointInterval hitPoints;
    auto primitive = box.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Box, ShouldNotIntersectIfBoxIsBehindRayOrigin) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Rayd ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = box.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.minWithPositiveDistance().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Box, ShouldReportBothHitpointsWhenRayOriginIsInsideBox) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    Rayd ray(Vector3d(0, 0, 0), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = box.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &box);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(-1, hitPoints.min().distance());

    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().point());
    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().normal());
    ASSERT_EQ(1, hitPoints.max().distance());

    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(Box, ShouldIntersectRay4PacketLikeScalarRays) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    const std::array<Rayf, 4> rayArray{
      Rayf(Vector3f(0, 0, -2), Vector3f(0, 0, 1)), Rayf(Vector3f(0, 0, -2), Vector3f(0, 1, 0)),
      Rayf(Vector3f(0, 0, 2), Vector3f(0, 0, 1)), Rayf(Vector3f(0, 0, 0), Vector3f(0, 0, 1))};

    State packetState;
    const auto result = box.intersectPacket(Ray4(rayArray), packetState);

    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      State scalarState;
      HitPointInterval hitPoints;
      const auto primitive = box.intersect(Rayd(rayArray[lane]), hitPoints, scalarState);
      ASSERT_EQ(primitive != nullptr, result.hit(lane)) << "lane " << lane;
      if (primitive != nullptr) {
        ASSERT_NEAR(hitPoints.min().distance(), result.tNear[lane], 1e-5) << "lane " << lane;
        ASSERT_NEAR(hitPoints.max().distance(), result.tFar[lane], 1e-5) << "lane " << lane;
      }
    }
    ASSERT_EQ(2, packetState.intersectionHits);
    ASSERT_EQ(2, packetState.intersectionMisses);
  }

  TEST(Box, ShouldMaterializeRay4PacketHits) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    const std::array<Rayd, 4> rayArray{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, 0), Vector3d(0, 0, 1))};
    PacketStates4 ps;

    const auto result = box.intersectPacketHits(Ray4(rayArray), ps.states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&box, result.primitive(0));
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).normal());
    EXPECT_EQ(1, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    ASSERT_TRUE(result.hit(3));
    EXPECT_EQ(Vector3d(0, 0, 1), result.hitPoint(3).point());
    EXPECT_EQ(Vector3d(0, 0, 1), result.hitPoint(3).normal());
    EXPECT_EQ(1, result.hitPoint(3).distance());
    EXPECT_EQ(1, ps.lanes[0].intersectionHits);
    EXPECT_EQ(1, ps.lanes[1].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[2].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[3].intersectionHits);
  }

  TEST(Box, ShouldMaterializeRay4PacketIntervals) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    const std::array<Rayd, 4> rayArray{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, 0), Vector3d(0, 0, 1))};
    PacketStates4 ps;

    const auto result = box.intersectPacketIntervals(Ray4(rayArray), ps.states);

    ASSERT_TRUE(result.hit(0));
    ASSERT_TRUE(result.hasInterval(0));
    EXPECT_EQ(&box, result.primitive(0));
    EXPECT_EQ(1, result.interval(0).min().distance());
    EXPECT_EQ(3, result.interval(0).max().distance());
    EXPECT_FALSE(result.scalarFallback(0));

    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hasInterval(1));

    EXPECT_FALSE(result.hit(2));
    ASSERT_TRUE(result.hasInterval(2));
    EXPECT_EQ(-3, result.interval(2).min().distance());
    EXPECT_EQ(-1, result.interval(2).max().distance());
    EXPECT_FALSE(result.scalarFallback(2));

    ASSERT_TRUE(result.hit(3));
    ASSERT_TRUE(result.hasInterval(3));
    EXPECT_EQ(-1, result.interval(3).min().distance());
    EXPECT_EQ(1, result.interval(3).max().distance());
    EXPECT_FALSE(result.scalarFallback(3));

    EXPECT_EQ(1, ps.lanes[0].intersectionHits);
    EXPECT_EQ(1, ps.lanes[1].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[2].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[3].intersectionHits);
    for (const auto& state : ps.lanes) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(Box, ShouldMaterializeRay8PacketHits) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    const std::array<Rayd, 8> rayArray{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, 1)),  Rayd(Vector3d(0, 0, 0), Vector3d(0, 0, 1)),
      Rayd(Vector3d(-2, 0, 0), Vector3d(1, 0, 0)), Rayd(Vector3d(2, 0, 0), Vector3d(1, 0, 0)),
      Rayd(Vector3d(0, -2, 0), Vector3d(0, 1, 0)), Rayd(Vector3d(0, 2, 0), Vector3d(0, -1, 0))};
    PacketStates8 ps;

    const auto result = box.intersectPacketHits(Ray8(rayArray), ps.states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&box, result.primitive(0));
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).normal());
    EXPECT_EQ(1, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    ASSERT_TRUE(result.hit(3));
    EXPECT_EQ(Vector3d(0, 0, 1), result.hitPoint(3).point());
    ASSERT_TRUE(result.hit(4));
    EXPECT_EQ(Vector3d(-1, 0, 0), result.hitPoint(4).point());
    EXPECT_EQ(Vector3d(-1, 0, 0), result.hitPoint(4).normal());
    EXPECT_FALSE(result.hit(5));
    ASSERT_TRUE(result.hit(6));
    EXPECT_EQ(Vector3d(0, -1, 0), result.hitPoint(6).point());
    ASSERT_TRUE(result.hit(7));
    EXPECT_EQ(Vector3d(0, 1, 0), result.hitPoint(7).point());
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

  TEST(Box, ShouldReturnFarthestPoint) {
    Box box(Vector3d(), Vector3d(1, 1, 1));
    auto direction = Vector3d(0.1, -0.1, 0.1).normalized();
    auto expected = Vector3d(1, -1, 1);

    ASSERT_EQ(expected, box.farthestPoint(direction));
  }

  TEST(Box, ShouldReturnBoundingBox) {
    Box box(Vector3d::null, Vector3d(1, 1, 1));
    BoundingBoxd bbox = box.boundingBox();
    ASSERT_EQ(Vector3d(-1, -1, -1), bbox.min());
    ASSERT_EQ(Vector3d(1, 1, 1), bbox.max());
  }
}
