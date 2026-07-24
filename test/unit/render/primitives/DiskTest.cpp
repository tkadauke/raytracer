#include "gtest/gtest.h"
#include "render/State.h"
#include "render/primitives/Disk.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include "test/helpers/PrimitiveTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

namespace DiskTest {
  using namespace render;
  using test::helpers::PacketStates4;
  using test::helpers::PacketStates8;

  TEST(Disk, ShouldInitializeWithValues) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
  }

  TEST(Disk, ShouldIntersectWithRay) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = disk.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &disk);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(2, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(Disk, ShouldNotIntersectWithMissingRay) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 1, 0));

    State state;
    HitPointInterval hitPoints;
    auto primitive = disk.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Disk, ShouldNotIntersectWithCoplanarParallelRay) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto primitive = disk.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Disk, ShouldNotIntersectIfDiskIsBehindRayOrigin) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    Rayd ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = disk.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_FALSE(hitPoints.min().isUndefined());
    ASSERT_TRUE(hitPoints.minWithPositiveDistance().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Disk, ShouldMaterializeRay4PacketHits) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(2, 0, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 2, -2), Vector3d(0, 0, 1))});
    PacketStates4 ps;

    const auto result = disk.intersectPacketHits(rays, ps.states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&disk, result.primitive(0));
    EXPECT_EQ(Vector3d(0, 0, 0), result.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).normal());
    EXPECT_EQ(2, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    EXPECT_FALSE(result.hit(3));
    EXPECT_EQ(1, ps.lanes[0].intersectionHits);
    EXPECT_EQ(1, ps.lanes[1].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[2].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[3].intersectionMisses);
  }

  TEST(Disk, ShouldRejectCoplanarParallelRay4PacketHits) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, 0), Vector3d(1, 0, 0)), Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(2, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, 1))});
    PacketStates4 ps;

    const auto result = disk.intersectPacketHits(rays, ps.states);

    EXPECT_FALSE(result.hit(0));
    ASSERT_TRUE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    EXPECT_FALSE(result.hit(3));
    EXPECT_EQ(1, ps.lanes[0].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[1].intersectionHits);
    EXPECT_EQ(1, ps.lanes[2].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[3].intersectionMisses);
  }

  TEST(Disk, ShouldMaterializeRay8PacketHits) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    const Ray8 rays(std::array<Rayd, Ray8::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(2, 0, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 2, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0.5, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0.5, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(-0.5, 0, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, -0.5, -2), Vector3d(0, 0, 1))});
    PacketStates8 ps;

    const auto result = disk.intersectPacketHits(rays, ps.states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&disk, result.primitive(0));
    EXPECT_EQ(Vector3d(0, 0, 0), result.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).normal());
    EXPECT_EQ(2, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    EXPECT_FALSE(result.hit(3));
    ASSERT_TRUE(result.hit(4));
    EXPECT_EQ(Vector3d(0.5, 0, 0), result.hitPoint(4).point());
    ASSERT_TRUE(result.hit(5));
    EXPECT_EQ(Vector3d(0, 0.5, 0), result.hitPoint(5).point());
    ASSERT_TRUE(result.hit(6));
    EXPECT_EQ(Vector3d(-0.5, 0, 0), result.hitPoint(6).point());
    ASSERT_TRUE(result.hit(7));
    EXPECT_EQ(Vector3d(0, -0.5, 0), result.hitPoint(7).point());
    EXPECT_EQ(1, ps.lanes[0].intersectionHits);
    EXPECT_EQ(1, ps.lanes[1].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[2].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[3].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[4].intersectionHits);
    EXPECT_EQ(1, ps.lanes[5].intersectionHits);
    EXPECT_EQ(1, ps.lanes[6].intersectionHits);
    EXPECT_EQ(1, ps.lanes[7].intersectionHits);
    for (const auto& state : ps.lanes) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(Disk, ShouldReturnFarthestPoint) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    auto direction = Vector3d(1, 1, 1).normalized();
    auto expected = Vector3d(1, 1, 0).normalized();

    ASSERT_VECTOR_NEAR(expected, disk.farthestPoint(direction), 0.001);
  }

  TEST(Disk, ShouldReturnBoundingBox) {
    Disk disk(Vector3d(), Vector3d(0, 0, -1), 1);
    BoundingBoxd bbox = disk.boundingBox();
    ASSERT_EQ(Vector3d(-1, -1, -1), bbox.min());
    ASSERT_EQ(Vector3d(1, 1, 1), bbox.max());
  }
}
