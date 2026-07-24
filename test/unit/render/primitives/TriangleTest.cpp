#include <gtest/gtest.h>
#include "render/State.h"
#include "render/primitives/Triangle.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include "test/helpers/PrimitiveTestHelper.h"

namespace TriangleTest {
  using namespace ::testing;
  using namespace render;
  using test::helpers::PacketStates4;
  using test::helpers::PacketStates8;

  struct TriangleTest : public ::testing::Test {
    void SetUp() {
      point0 = Vector3d(-1, -1, 0);
      point1 = Vector3d(-1, 1, 0);
      point2 = Vector3d(1, -1, 0);
    }

    Vector3d point0, point1, point2;
  };

  TEST_F(TriangleTest, ShouldInitializeWithValues) {
    Triangle triangle(this->point0, this->point1, this->point2);
  }

  TEST_F(TriangleTest, ShouldIntersectWithRay) {
    Triangle triangle(this->point0, this->point1, this->point2);
    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = triangle.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &triangle);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST_F(TriangleTest, ShouldNotIntersectWithMissingRay) {
    Triangle triangle(this->point0, this->point1, this->point2);
    Rayd ray(Vector3d(0, 4, -1), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = triangle.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST_F(TriangleTest, ShouldNotIntersectIfPointIsBehindRayOrigin) {
    Triangle triangle(this->point0, this->point1, this->point2);
    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, -1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = triangle.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.minWithPositiveDistance().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST_F(TriangleTest, ShouldIntersectRay4PacketLikeScalarRays) {
    Triangle triangle(this->point0, this->point1, this->point2);
    const std::array<Rayf, 4> rayArray{Rayf(Vector3f(0, 0, -1), Vector3f(0, 0, 1)),
                                       Rayf(Vector3f(0, 4, -1), Vector3f(0, 0, 1)),
                                       Rayf(Vector3f(0, 0, -1), Vector3f(0, 0, -1)),
                                       Rayf(Vector3f(-0.5f, -0.5f, -1), Vector3f(0, 0, 1))};

    State packetState;
    const auto result = triangle.intersectPacket(Ray4(rayArray), packetState);

    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      State scalarState;
      HitPointInterval hitPoints;
      const auto primitive = triangle.intersect(Rayd(rayArray[lane]), hitPoints, scalarState);
      ASSERT_EQ(primitive != nullptr, result.hit(lane)) << "lane " << lane;
      if (primitive != nullptr) {
        ASSERT_NEAR(hitPoints.min().distance(), result.tNear[lane], 1e-5) << "lane " << lane;
      }
    }
    ASSERT_EQ(2, packetState.intersectionHits);
    ASSERT_EQ(2, packetState.intersectionMisses);
  }

  TEST_F(TriangleTest, ShouldMaterializeRay4PacketHits) {
    Triangle triangle(this->point0, this->point1, this->point2);
    const std::array<Rayd, 4> rayArray{Rayd(Vector3d(0, 0, -1), Vector3d(0, 0, 1)),
                                       Rayd(Vector3d(0, 4, -1), Vector3d(0, 0, 1)),
                                       Rayd(Vector3d(0, 0, -1), Vector3d(0, 0, -1)),
                                       Rayd(Vector3d(-0.5, -0.5, -1), Vector3d(0, 0, 1))};
    PacketStates4 ps;

    const auto result = triangle.intersectPacketHits(Ray4(rayArray), ps.states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&triangle, result.primitive(0));
    EXPECT_EQ(Vector3d(0, 0, 0), result.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).normal());
    EXPECT_EQ(1, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    ASSERT_TRUE(result.hit(3));
    EXPECT_EQ(Vector3d(-0.5, -0.5, 0), result.hitPoint(3).point());
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(3).normal());
    EXPECT_EQ(1, result.hitPoint(3).distance());
    EXPECT_EQ(1, ps.lanes[0].intersectionHits);
    EXPECT_EQ(1, ps.lanes[1].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[2].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[3].intersectionHits);
  }

  TEST_F(TriangleTest, ShouldMaterializeRay8PacketHits) {
    Triangle triangle(this->point0, this->point1, this->point2);
    const std::array<Rayd, 8> rayArray{Rayd(Vector3d(0, 0, -1), Vector3d(0, 0, 1)),
                                       Rayd(Vector3d(0, 4, -1), Vector3d(0, 0, 1)),
                                       Rayd(Vector3d(0, 0, -1), Vector3d(0, 0, -1)),
                                       Rayd(Vector3d(-0.5, -0.5, -1), Vector3d(0, 0, 1)),
                                       Rayd(Vector3d(-0.75, 0.75, -1), Vector3d(0, 0, 1)),
                                       Rayd(Vector3d(0.75, 0.75, -1), Vector3d(0, 0, 1)),
                                       Rayd(Vector3d(1.1, -0.75, -1), Vector3d(0, 0, 1)),
                                       Rayd(Vector3d(-0.25, -0.25, -2), Vector3d(0, 0, 2))};
    PacketStates8 ps;

    const auto result = triangle.intersectPacketHits(Ray8(rayArray), ps.states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&triangle, result.primitive(0));
    EXPECT_EQ(Vector3d(0, 0, 0), result.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).normal());
    EXPECT_EQ(1, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    ASSERT_TRUE(result.hit(3));
    EXPECT_EQ(Vector3d(-0.5, -0.5, 0), result.hitPoint(3).point());
    ASSERT_TRUE(result.hit(4));
    EXPECT_EQ(Vector3d(-0.75, 0.75, 0), result.hitPoint(4).point());
    EXPECT_FALSE(result.hit(5));
    EXPECT_FALSE(result.hit(6));
    ASSERT_TRUE(result.hit(7));
    EXPECT_EQ(Vector3d(-0.25, -0.25, 0), result.hitPoint(7).point());
    EXPECT_EQ(1, ps.lanes[0].intersectionHits);
    EXPECT_EQ(1, ps.lanes[1].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[2].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[3].intersectionHits);
    EXPECT_EQ(1, ps.lanes[4].intersectionHits);
    EXPECT_EQ(1, ps.lanes[5].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[6].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[7].intersectionHits);
    for (const auto& state : ps.lanes) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST_F(TriangleTest, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    Triangle triangle(this->point0, this->point1, this->point2);
    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));

    State state;
    ASSERT_TRUE(triangle.intersects(ray, state));
  }

  TEST_F(TriangleTest, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Triangle triangle(this->point0, this->point1, this->point2);
    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, -1));

    State state;
    ASSERT_FALSE(triangle.intersects(ray, state));
  }

  TEST_F(TriangleTest, ShouldHaveSameNormalEverywhere) {
    Triangle triangle(this->point0, this->point1, this->point2);
    State state;
    HitPointInterval hitPoints1;
    Rayd ray1(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    triangle.intersect(ray1, hitPoints1, state);
    Vector3d normal1 = hitPoints1.min().normal();

    HitPointInterval hitPoints2;
    Rayd ray2(Vector3d(-1, -1, -1), Vector3d(0, 0, 1));
    triangle.intersect(ray2, hitPoints2, state);
    Vector3d normal2 = hitPoints2.min().normal();

    ASSERT_EQ(normal1, normal2);
  }

  TEST_F(TriangleTest, ShouldReturnBoundingBox) {
    Triangle triangle(this->point0, this->point1, this->point2);
    ASSERT_EQ(BoundingBoxd(Vector3d(-1, -1, 0), Vector3d(1, 1, 0)), triangle.boundingBox());
  }
}
