#include "gtest/gtest.h"
#include "core/math/RayPacket.h"

#include <array>

namespace RayPacketTest {

  TEST(Ray4, ShouldConstructFromFourRaysAndExtractLanes) {
    const std::array<Rayf, 4> rays{Rayf(Vector3f(1, 2, 3), Vector3f(4, 5, 6)),
                                   Rayf(Vector3f(7, 8, 9), Vector3f(10, 11, 12)),
                                   Rayf(Vector3f(13, 14, 15), Vector3f(16, 17, 18)),
                                   Rayf(Vector3f(19, 20, 21), Vector3f(22, 23, 24))};

    const Ray4 packet(rays);

    static_assert(alignof(Ray4) == 16, "Ray4 must be SSE aligned");
    ASSERT_EQ(1, packet.originX[0]);
    ASSERT_EQ(8, packet.originY[1]);
    ASSERT_EQ(15, packet.originZ[2]);
    ASSERT_EQ(22, packet.directionX[3]);
    ASSERT_EQ(rays[2].origin(), packet.ray(2).origin());
    ASSERT_EQ(rays[2].direction(), packet.ray(2).direction());
  }

  TEST(Ray8, ShouldConstructFromEightRaysAndExtractLanes) {
    std::array<Rayf, 8> rays{Rayf(Vector3f(1, 2, 3), Vector3f(4, 5, 6)),
                             Rayf(Vector3f(7, 8, 9), Vector3f(10, 11, 12)),
                             Rayf(Vector3f(13, 14, 15), Vector3f(16, 17, 18)),
                             Rayf(Vector3f(19, 20, 21), Vector3f(22, 23, 24)),
                             Rayf(Vector3f(25, 26, 27), Vector3f(28, 29, 30)),
                             Rayf(Vector3f(31, 32, 33), Vector3f(34, 35, 36)),
                             Rayf(Vector3f(37, 38, 39), Vector3f(40, 41, 42)),
                             Rayf(Vector3f(43, 44, 45), Vector3f(46, 47, 48))};

    const Ray8 packet(rays);

    static_assert(alignof(Ray8) == 32, "Ray8 must be AVX aligned");
    ASSERT_EQ(25, packet.originX[4]);
    ASSERT_EQ(32, packet.originY[5]);
    ASSERT_EQ(39, packet.originZ[6]);
    ASSERT_EQ(46, packet.directionX[7]);
    ASSERT_EQ(rays[6].origin(), packet.ray(6).origin());
    ASSERT_EQ(rays[6].direction(), packet.ray(6).direction());
  }

  TEST(Ray4, ShouldPreservePreciseDoubleRaysForMaterialization) {
    const std::array<Rayd, 4> rays{
      Rayd(Vector3d(1.0 / 3.0, 2.0 / 3.0, 3.0 / 7.0), Vector3d(4.0 / 9.0, 5.0 / 11.0, 6.0 / 13.0)),
      Rayd(Vector3d(7.0 / 17.0, 8.0 / 19.0, 9.0 / 23.0),
           Vector3d(10.0 / 29.0, 11.0 / 31.0, 12.0 / 37.0)),
      Rayd(Vector3d(13.0 / 41.0, 14.0 / 43.0, 15.0 / 47.0),
           Vector3d(16.0 / 53.0, 17.0 / 59.0, 18.0 / 61.0)),
      Rayd(Vector3d(19.0 / 67.0, 20.0 / 71.0, 21.0 / 73.0),
           Vector3d(22.0 / 79.0, 23.0 / 83.0, 24.0 / 89.0))};

    const Ray4 packet(rays);
    const Rayd precise = packet.rayd(2);

    EXPECT_NE(static_cast<double>(packet.originX[2]), rays[2].origin().x());
    EXPECT_DOUBLE_EQ(rays[2].origin().x(), precise.origin().x());
    EXPECT_DOUBLE_EQ(rays[2].origin().y(), precise.origin().y());
    EXPECT_DOUBLE_EQ(rays[2].origin().z(), precise.origin().z());
    EXPECT_DOUBLE_EQ(rays[2].direction().x(), precise.direction().x());
    EXPECT_DOUBLE_EQ(rays[2].direction().y(), precise.direction().y());
    EXPECT_DOUBLE_EQ(rays[2].direction().z(), precise.direction().z());
  }

  TEST(RayPacketIntersection4, ShouldTrackHitMaskAndDistances) {
    RayPacketIntersection4 result;

    ASSERT_FALSE(result.hit(2));
    result.setHit(2, 1.5f, 3.5f);

    ASSERT_TRUE(result.hit(2));
    ASSERT_EQ(0b0100, result.hitMask);
    ASSERT_EQ(1.5f, result.tNear[2]);
    ASSERT_EQ(3.5f, result.tFar[2]);
  }
}
