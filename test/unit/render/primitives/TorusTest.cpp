#include "gtest/gtest.h"
#include "render/State.h"
#include "render/primitives/Torus.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include "test/helpers/VectorTestHelper.h"

namespace TorusTest {
  using namespace render;
  using namespace render;
  using namespace render;

  TEST(Torus, ShouldInitializeWithValues) {
    Torus torus(2, 1);
  }

  TEST(Torus, ShouldIntersectWithRay) {
    Torus torus(2, 1);
    Rayd ray(Vector3d(0, 0, -4), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = torus.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &torus);
    ASSERT_EQ(Vector3d(0, 0, -3), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(4u, hitPoints.points().size());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(Torus, ShouldIntersectAtGrazingIncidence) {
    Torus torus(2, 1);
    Rayd ray(Vector3d(0, 0.999999, -4), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = torus.intersect(ray, hitPoints, state);

    ASSERT_EQ(primitive, &torus);
    ASSERT_EQ(4u, hitPoints.points().size());
    auto points = hitPoints.points().begin();
    EXPECT_NEAR(1.99858578679, points[0].point.distance(), 1e-8);
    EXPECT_NEAR(2.00141421321, points[1].point.distance(), 1e-8);
    EXPECT_NEAR(5.99858578679, points[2].point.distance(), 1e-8);
    EXPECT_NEAR(6.00141421321, points[3].point.distance(), 1e-8);
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(Torus, ShouldNotIntersectWithMissingRay) {
    Torus torus(2, 1);
    Rayd ray(Vector3d(0, 0, -4), Vector3d(0, 1, 0));

    State state;
    HitPointInterval hitPoints;
    auto primitive = torus.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Torus, ShouldNotIntersectIfTorusIsBehindRayOrigin) {
    Torus torus(2, 1);
    Rayd ray(Vector3d(0, 0, 4), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = torus.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Torus, ShouldNotIntersectWithTorusHole) {
    Torus torus(2, 1);
    Rayd ray(Vector3d(0, 4, 0), Vector3d(0, -1, 0));

    State state;
    HitPointInterval hitPoints;
    auto primitive = torus.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Torus, ShouldMaterializeRay4PacketHits) {
    Torus torus(2, 1);
    const Ray4 rays(std::array<Rayd, 4>{
      Rayd(Vector3d(0, 0, -4), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, -4), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 4), Vector3d(0, 0, 1)), Rayd(Vector3d(), Vector3d(0, 0, 1))});
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    const auto result = torus.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&torus, result.primitive(0));
    EXPECT_EQ(Vector3d(0, 0, -3), result.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).normal());
    EXPECT_EQ(1, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    ASSERT_TRUE(result.hit(3));
    ASSERT_VECTOR_NEAR(Vector4d(0, 0, 1, 1), result.hitPoint(3).point(), 1e-12);
    ASSERT_VECTOR_NEAR(Vector3d(0, 0, -1), result.hitPoint(3).normal(), 1e-12);
    EXPECT_NEAR(1, result.hitPoint(3).distance(), 1e-12);
    EXPECT_EQ(1, laneStates[0].intersectionHits);
    EXPECT_EQ(1, laneStates[1].intersectionMisses);
    EXPECT_EQ(1, laneStates[2].intersectionMisses);
    EXPECT_EQ(1, laneStates[3].intersectionHits);
    for (const State& state : laneStates) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(Torus, ShouldReturnTrueForIntersectsIfThereIsAIntersectionWithRay) {
    Torus torus(2, 1);
    Rayd ray(Vector3d(0, 0, -4), Vector3d(0, 0, 1));

    State state;
    ASSERT_TRUE(torus.intersects(ray, state));
  }

  TEST(Torus, ShouldReturnFalseForIntersectsWithMissingRay) {
    Torus torus(2, 1);
    Rayd ray(Vector3d(0, 0, -4), Vector3d(0, 1, 0));

    State state;
    ASSERT_FALSE(torus.intersects(ray, state));
  }

  TEST(Torus, ShouldReturnFalseForIntersectsIfTorusIsBehindRayOrigin) {
    Torus torus(2, 1);
    Rayd ray(Vector3d(0, 0, 4), Vector3d(0, 0, 1));

    State state;
    ASSERT_FALSE(torus.intersects(ray, state));
  }

  TEST(Torus, ShouldReturnTrueForIntersectsIfRayIsInsideTorus) {
    Torus torus(2, 1);
    Rayd ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));

    State state;
    ASSERT_TRUE(torus.intersects(ray, state));
  }

  TEST(Torus, ShouldReturnTrueForIntersectsIfRayIsInsideTorusHole) {
    Torus torus(2, 1);
    Rayd ray(Vector3d(), Vector3d(0, 0, 1));

    State state;
    ASSERT_TRUE(torus.intersects(ray, state));
  }

  TEST(Torus, ShouldReturnBoundingBox) {
    Torus torus(2, 1);
    ASSERT_EQ(BoundingBoxd(Vector3d(-3, -1, -3), Vector3d(3, 1, 3)), torus.boundingBox());
  }
}
