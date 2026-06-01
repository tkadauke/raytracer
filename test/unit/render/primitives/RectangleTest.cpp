#include "gtest/gtest.h"
#include "render/State.h"
#include "render/primitives/Rectangle.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"

namespace RectangleTest {
  using namespace render;
  using namespace render;
  using namespace render;

  TEST(Rectangle, ShouldInitializeWithValues) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
  }

  TEST(Rectangle, ShouldInitializeWithNormal) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0),
                        Vector3d(0, 0, 1));
  }

  TEST(Rectangle, ShouldIntersectWithRay) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = rectangle.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &rectangle);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.min().normal());
    ASSERT_EQ(2, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(Rectangle, ShouldNotIntersectWithParallelRay) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    Rayd ray(Vector3d(0, 0, -2), Vector3d(1, 1, 0));

    State state;
    HitPointInterval hitPoints;
    auto primitive = rectangle.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Rectangle, ShouldNotIntersectWithMissingRay) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    Rayd ray(Vector3d(-2, -2, -2), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = rectangle.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Rectangle, ShouldNotIntersectIfRectangleIsBehindRayOrigin) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    Rayd ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = rectangle.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.minWithPositiveDistance().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Rectangle, ShouldMaterializeRay4PacketHits) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(2, 0, 0), Vector3d(0, 2, 0));
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, -2), Vector3d(1, 0, 0)),
      Rayd(Vector3d(2, 2, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, 1))});
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    const auto result = rectangle.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&rectangle, result.primitive(0));
    EXPECT_EQ(Vector3d(0, 0, 0), result.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 0, 1), result.hitPoint(0).normal());
    EXPECT_EQ(2, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    EXPECT_FALSE(result.hit(3));
    EXPECT_EQ(1, laneStates[0].intersectionHits);
    EXPECT_EQ(1, laneStates[1].intersectionMisses);
    EXPECT_EQ(1, laneStates[2].intersectionMisses);
    EXPECT_EQ(1, laneStates[3].intersectionMisses);
  }

  TEST(Rectangle, ShouldReturnBoundingBox) {
    Rectangle rectangle(Vector3d(-1, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    BoundingBoxd expected = BoundingBoxd(Vector3d(-1, -1, 0), Vector3d(0, 0, 0)).grownByEpsilon();

    ASSERT_EQ(expected, rectangle.boundingBox());
  }
}
