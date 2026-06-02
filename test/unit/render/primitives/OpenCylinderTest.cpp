#include "gtest/gtest.h"
#include "render/State.h"
#include "render/primitives/OpenCylinder.h"
#include "core/DivisionByZeroException.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include "test/helpers/VectorTestHelper.h"

namespace OpenCylinderTest {
  using namespace render;
  using namespace render;
  using namespace render;

  TEST(OpenCylinder, ShouldInitializeWithValues) {
    OpenCylinder cylinder(1, 2);
  }

  TEST(OpenCylinder, ShouldThrowDivisionByZeroExceptionWhenConstructedWithZeroRadius) {
    ASSERT_THROW(OpenCylinder(0, 2), DivisionByZeroException);
  }

  TEST(OpenCylinder, ShouldIntersectWithRayInXDirection) {
    OpenCylinder cylinder(1, 2);
    Rayd ray(Vector3d(-2, 0, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto primitive = cylinder.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &cylinder);
    ASSERT_EQ(Vector3d(-1, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(-1, 0, 0), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(OpenCylinder, ShouldNotIntersectWithRayInYDirection) {
    OpenCylinder cylinder(1, 2);
    Rayd ray(Vector3d(0, -2, 0), Vector3d(0, 1, 0));

    State state;
    HitPointInterval hitPoints;
    auto primitive = cylinder.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(OpenCylinder, ShouldIntersectWithRayInZDirection) {
    OpenCylinder cylinder(1, 2);
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = cylinder.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &cylinder);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(OpenCylinder, ShouldIntersectIfRayIsTangentToPrimitive) {
    OpenCylinder cylinder(1, 2);
    Rayd ray(Vector3d(0, 1, -2), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = cylinder.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &cylinder);
    ASSERT_EQ(Vector3d(0, 1, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(OpenCylinder, ShouldNotIntersectWithMissingRay) {
    OpenCylinder cylinder(1, 2);
    Rayd ray(Vector3d(0, 0, -3), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto primitive = cylinder.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(OpenCylinder, ShouldNotIntersectIfOpenCylinderIsBehindRayOrigin) {
    OpenCylinder cylinder(1, 2);
    Rayd ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = cylinder.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.minWithPositiveDistance().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(OpenCylinder, ShouldReportBothHitpointsWhenRayOriginIsInsideOpenCylinder) {
    OpenCylinder cylinder(1, 2);
    Rayd ray(Vector3d(0, 0, 0), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = cylinder.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &cylinder);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(-1, hitPoints.min().distance());

    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().point());
    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().normal());
    ASSERT_EQ(1, hitPoints.max().distance());

    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(OpenCylinder, ShouldMaterializeRay4PacketHits) {
    OpenCylinder cylinder(1, 2);
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, -2, 0), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, 0), Vector3d(0, 0, 1))});
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    const auto result = cylinder.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&cylinder, result.primitive(0));
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).normal());
    EXPECT_EQ(1, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    ASSERT_TRUE(result.hit(3));
    EXPECT_EQ(&cylinder, result.primitive(3));
    EXPECT_EQ(Vector3d(0, 0, 1), result.hitPoint(3).point());
    EXPECT_EQ(Vector3d(0, 0, 1), result.hitPoint(3).normal());
    EXPECT_EQ(1, result.hitPoint(3).distance());
    EXPECT_EQ(1, laneStates[0].intersectionHits);
    EXPECT_EQ(1, laneStates[1].intersectionMisses);
    EXPECT_EQ(1, laneStates[2].intersectionMisses);
    EXPECT_EQ(1, laneStates[3].intersectionHits);
  }

  TEST(OpenCylinder, ShouldMaterializeRay4PacketIntervals) {
    OpenCylinder cylinder(1, 2);
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, -2, 0), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, 0), Vector3d(0, 0, 1))});
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    const auto result = cylinder.intersectPacketIntervals(rays, states);

    ASSERT_TRUE(result.hit(0));
    ASSERT_TRUE(result.hasInterval(0));
    EXPECT_EQ(&cylinder, result.primitive(0));
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

    EXPECT_EQ(1, laneStates[0].intersectionHits);
    EXPECT_EQ(1, laneStates[1].intersectionMisses);
    EXPECT_EQ(1, laneStates[2].intersectionMisses);
    EXPECT_EQ(1, laneStates[3].intersectionHits);
    for (const auto& state : laneStates) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(OpenCylinder, ShouldMaterializeRay8PacketHits) {
    OpenCylinder cylinder(1, 2);
    const Ray8 rays(std::array<Rayd, Ray8::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, -2, 0), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, 0), Vector3d(0, 0, 1)),
      Rayd(Vector3d(-2, 0, 0), Vector3d(1, 0, 0)), Rayd(Vector3d(2, 0, 0), Vector3d(1, 0, 0)),
      Rayd(Vector3d(0, 2, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0.5, -2), Vector3d(0, 0, 1))});
    std::array<State, Ray8::lanes> laneStates;
    PrimitivePacketState8 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3],
                                 &laneStates[4], &laneStates[5], &laneStates[6], &laneStates[7]};

    const auto result = cylinder.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&cylinder, result.primitive(0));
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).normal());
    EXPECT_EQ(1, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    ASSERT_TRUE(result.hit(3));
    EXPECT_EQ(Vector3d(0, 0, 1), result.hitPoint(3).point());
    EXPECT_EQ(Vector3d(0, 0, 1), result.hitPoint(3).normal());
    ASSERT_TRUE(result.hit(4));
    EXPECT_EQ(Vector3d(-1, 0, 0), result.hitPoint(4).point());
    EXPECT_EQ(Vector3d(-1, 0, 0), result.hitPoint(4).normal());
    EXPECT_FALSE(result.hit(5));
    EXPECT_FALSE(result.hit(6));
    ASSERT_TRUE(result.hit(7));
    EXPECT_EQ(Vector3d(0, 0.5, -1), result.hitPoint(7).point());
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(7).normal());
    EXPECT_EQ(1, laneStates[0].intersectionHits);
    EXPECT_EQ(1, laneStates[1].intersectionMisses);
    EXPECT_EQ(1, laneStates[2].intersectionMisses);
    EXPECT_EQ(1, laneStates[3].intersectionHits);
    EXPECT_EQ(1, laneStates[4].intersectionHits);
    EXPECT_EQ(1, laneStates[5].intersectionMisses);
    EXPECT_EQ(1, laneStates[6].intersectionMisses);
    EXPECT_EQ(1, laneStates[7].intersectionHits);
    for (const auto& state : laneStates) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(OpenCylinder, ShouldReturnFarthestPoint) {
    OpenCylinder cylinder(1, 2);
    auto direction = Vector3d(1, 0.1, 1).normalized();
    auto expected = Vector3d(1, 0, 1).normalized();
    expected.setY(1);

    ASSERT_VECTOR_NEAR(expected, cylinder.farthestPoint(direction), 0.001);
  }

  TEST(OpenCylinder, ShouldReturnBoundingBox) {
    OpenCylinder cylinder(1, 2);
    BoundingBoxd bbox = cylinder.boundingBox();
    ASSERT_EQ(Vector3d(-1, -1, -1), bbox.min());
    ASSERT_EQ(Vector3d(1, 1, 1), bbox.max());
  }
}
