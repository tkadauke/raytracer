#include <gtest/gtest.h>

#include "core/math/HitPointInterval.h"
#include "render/State.h"
#include "render/primitives/BVH.h"
#include "render/primitives/Sphere.h"

#include <array>
#include <cmath>
#include <memory>
#include <random>

namespace BVHTest {
  using namespace ::testing;
  using namespace render;

  // Helper: a `BVH` populated with N axis-aligned spheres on a 3D
  // grid. The grid layout makes hit/miss expectations trivial — a
  // ray along the X axis through y=z=0 hits the spheres at x=0, x=1,
  // x=2... in that order, and nothing else.
  static std::shared_ptr<BVH> gridSpheres(int sideLength, double spacing = 2.0) {
    auto bvh = std::make_shared<BVH>();
    for (int x = 0; x < sideLength; ++x) {
      for (int y = 0; y < sideLength; ++y) {
        for (int z = 0; z < sideLength; ++z) {
          bvh->add(std::make_shared<Sphere>(Vector3d(x * spacing, y * spacing, z * spacing), 0.5));
        }
      }
    }
    bvh->setup();
    return bvh;
  }

  static void expectPacketMatchesScalarLanes(const BVH& bvh, const std::array<Rayd, 4>& rays) {
    State packetState;
    const auto packet = bvh.intersectPacket(Ray4(rays), packetState);

    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      State scalarState;
      HitPointInterval scalarHits;
      const bool scalarHit = bvh.intersect(rays[lane], scalarHits, scalarState) != nullptr;

      ASSERT_EQ(scalarHit, packet.hit(lane)) << "lane " << lane;
      if (scalarHit) {
        const double scalarT = scalarHits.minWithPositiveDistance().distance();
        const double packetT = static_cast<double>(packet.tNear[lane]);
        EXPECT_NEAR(scalarT, packetT, 1e-3) << "lane " << lane;
      }
    }
  }

  static void expectPacketHitsMatchScalarLanes(const BVH& bvh, const std::array<Rayd, 4>& rays) {
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};
    const auto packet = bvh.intersectPacketHits(Ray4(rays), states);

    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      State scalarState;
      HitPointInterval scalarHits;
      const Primitive* scalarPrimitive = bvh.intersect(rays[lane], scalarHits, scalarState);

      ASSERT_EQ(scalarPrimitive != nullptr, packet.hit(lane)) << "lane " << lane;
      if (scalarPrimitive) {
        EXPECT_EQ(scalarPrimitive, packet.primitive(lane)) << "lane " << lane;
        EXPECT_NEAR(scalarHits.minWithPositiveDistance().distance(),
                    packet.hitPoint(lane).distance(), 1e-3)
          << "lane " << lane;
      }
    }
  }

  TEST(BVH, EmptyHierarchyMissesEverything) {
    BVH bvh;
    bvh.setup();
    Rayd ray(Vector3d(-10, 0, 0), Vector3d(1, 0, 0));
    State state;
    HitPointInterval hitPoints;
    EXPECT_EQ(nullptr, bvh.intersect(ray, hitPoints, state));
  }

  TEST(BVH, SinglePrimitiveHitForwardsToTheChild) {
    BVH bvh;
    auto sphere = std::make_shared<Sphere>(Vector3d::null, 1.0);
    bvh.add(sphere);
    bvh.setup();

    // Ray heading toward the sphere from -Z.
    Rayd ray(Vector3d(0, 0, -10), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hitPoints;
    auto hit = bvh.intersect(ray, hitPoints, state);

    ASSERT_EQ(sphere.get(), hit);
    EXPECT_FALSE(hitPoints.min().isUndefined());
  }

  TEST(BVH, ReturnsClosestHitAcrossMultiplePrimitives) {
    BVH bvh;
    auto near = std::make_shared<Sphere>(Vector3d(0, 0, -2), 0.5);
    auto far = std::make_shared<Sphere>(Vector3d(0, 0, 2), 0.5);
    bvh.add(near);
    bvh.add(far);
    bvh.setup();

    // Ray from -Z heading +Z hits both spheres; expect the near one.
    Rayd ray(Vector3d(0, 0, -10), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hitPoints;
    auto hit = bvh.intersect(ray, hitPoints, state);

    EXPECT_EQ(near.get(), hit);
  }

  TEST(BVH, MissAcrossManyPrimitives) {
    auto bvh = gridSpheres(4); // 64 spheres on a 4×4×4 grid

    // Ray well off the grid in the +y direction.
    Rayd ray(Vector3d(0, 1000, 0), Vector3d(1, 0, 0));
    State state;
    HitPointInterval hitPoints;
    EXPECT_EQ(nullptr, bvh->intersect(ray, hitPoints, state));
  }

  TEST(BVH, BuildAcceleratesAgainstLinearScan) {
    // Pin that the BVH produces the same hit as the linear scan for
    // many primitives + many random rays. This is the
    // correctness-equivalence test — performance equivalence is a
    // benchmark concern, not a unit-test one.
    constexpr int kSpheres = 8;
    auto bvh = gridSpheres(kSpheres);

    // Same primitives in a plain Composite.
    Composite linear;
    for (int x = 0; x < kSpheres; ++x) {
      for (int y = 0; y < kSpheres; ++y) {
        for (int z = 0; z < kSpheres; ++z) {
          linear.add(std::make_shared<Sphere>(Vector3d(x * 2.0, y * 2.0, z * 2.0), 0.5));
        }
      }
    }

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> coord(-5.0, 20.0);
    std::uniform_real_distribution<double> dir(-1.0, 1.0);

    int matches = 0;
    int total = 0;
    for (int i = 0; i < 100; ++i) {
      Vector3d origin(coord(rng), coord(rng), coord(rng));
      Vector3d direction(dir(rng), dir(rng), dir(rng));
      if (direction.length() < 1e-6)
        continue;
      direction = direction.normalized();
      Rayd ray(origin, direction);

      State sBvh, sLin;
      HitPointInterval pBvh, pLin;
      auto hBvh = bvh->intersect(ray, pBvh, sBvh);
      auto hLin = linear.intersect(ray, pLin, sLin);

      // Both must agree on hit-vs-miss. When both hit, the same
      // closest primitive must come back — pointer identity since
      // both share the same Sphere instances? They don't here (we
      // built two separate composites) — instead verify the
      // closest-distance match.
      ++total;
      if ((hBvh == nullptr) == (hLin == nullptr)) {
        if (hBvh && hLin) {
          const double tBvh = pBvh.minWithPositiveDistance().distance();
          const double tLin = pLin.minWithPositiveDistance().distance();
          if (std::abs(tBvh - tLin) < 1e-6)
            ++matches;
        } else {
          ++matches;
        }
      }
    }
    EXPECT_EQ(total, matches);
  }

  TEST(BVH, IntersectsShortCircuitsForShadowRays) {
    // The boolean intersects() variant should return true as soon as
    // any primitive reports a hit, without traversing the rest of
    // the tree. We can't easily test "didn't traverse" without
    // counters, but at minimum the correctness contract is the
    // same as the unaccelerated form.
    auto bvh = gridSpheres(3);
    Rayd ray(Vector3d(0, 0, -10), Vector3d(0, 0, 1));
    State state;
    EXPECT_TRUE(bvh->intersects(ray, state));
  }

  TEST(BVH, IntersectsMissesWhenRayDoesntHitAnyChild) {
    auto bvh = gridSpheres(3);
    Rayd ray(Vector3d(0, 1000, 0), Vector3d(1, 0, 0));
    State state;
    EXPECT_FALSE(bvh->intersects(ray, state));
  }

  TEST(BVH, FallsBackToLinearScanIfSetupNotCalled) {
    // If the user forgot to call setup(), intersect should still
    // produce correct (if slower) results via the inherited
    // Composite::intersect path.
    BVH bvh;
    auto sphere = std::make_shared<Sphere>(Vector3d::null, 1.0);
    bvh.add(sphere);
    // Note: no setup() call.

    Rayd ray(Vector3d(0, 0, -10), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hitPoints;
    EXPECT_EQ(sphere.get(), bvh.intersect(ray, hitPoints, state));
  }

  TEST(BVH, RespectsLeafSize) {
    // With a leaf size of 100, eight primitives should all fit in a
    // single leaf — the tree never splits. The intersect should
    // still produce correct results.
    BVH bvh;
    bvh.setLeafSize(100);
    for (int i = 0; i < 8; ++i) {
      bvh.add(std::make_shared<Sphere>(Vector3d(i * 2.0, 0, 0), 0.5));
    }
    bvh.setup();

    Rayd ray(Vector3d(-10, 0, 0), Vector3d(1, 0, 0));
    State state;
    HitPointInterval hitPoints;
    auto hit = bvh.intersect(ray, hitPoints, state);
    ASSERT_NE(nullptr, hit);
  }

  TEST(BVH, BoundingBoxCoversEveryChild) {
    BVH bvh;
    bvh.add(std::make_shared<Sphere>(Vector3d(-5, 0, 0), 1));
    bvh.add(std::make_shared<Sphere>(Vector3d(5, 0, 0), 1));
    bvh.setup();

    const auto& bbox = bvh.boundingBox();
    EXPECT_LE(bbox.min().x(), -6.0);
    EXPECT_GE(bbox.max().x(), 6.0);
  }

  // ── Packet traversal tests ──────────────────────────────────────────────

  TEST(BVH, PacketIntersectHitMaskMatchesScalarPath) {
    // Build a 4×4×4 grid of spheres and verify that intersectPacket(Ray4)
    // returns the same hit/miss result per lane as four scalar intersect
    // calls for the same rays.
    auto bvh = gridSpheres(4); // 64 spheres

    // Four rays aimed through the grid at known spheres.
    const std::array<Rayd, 4> testRays = {
      Rayd(Vector3d(-10, 0, 0), Vector3d(1, 0, 0)),  // hits x=0 sphere row
      Rayd(Vector3d(-10, 2, 0), Vector3d(1, 0, 0)),  // hits x=0,y=1 row
      Rayd(Vector3d(0, 1000, 0), Vector3d(1, 0, 0)), // misses (y=1000)
      Rayd(Vector3d(-10, 0, 2), Vector3d(1, 0, 0)),  // hits x=0,z=1 row
    };

    Ray4 packet(testRays);
    State packetState;
    const auto result = bvh->intersectPacket(packet, packetState);

    for (std::size_t i = 0; i < 4; ++i) {
      State scalarState;
      HitPointInterval hp;
      const bool scalarHit = (bvh->intersect(testRays[i], hp, scalarState) != nullptr);
      EXPECT_EQ(scalarHit, result.hit(i))
        << "Lane " << i << ": packet hit=" << result.hit(i) << " scalar hit=" << scalarHit;
    }
  }

  TEST(BVH, PacketIntersectTMinApproximatesScalarDistance) {
    // Verify that tNear from intersectPacket is close to the t returned
    // by scalar intersect for the same rays (within float precision).
    auto bvh = gridSpheres(4);

    const std::array<Rayd, 4> testRays = {
      Rayd(Vector3d(-10, 0, 0), Vector3d(1, 0, 0)),
      Rayd(Vector3d(-10, 2, 0), Vector3d(1, 0, 0)),
      Rayd(Vector3d(-10, 4, 0), Vector3d(1, 0, 0)),
      Rayd(Vector3d(-10, 6, 0), Vector3d(1, 0, 0)),
    };

    Ray4 packet(testRays);
    State packetState;
    const auto result = bvh->intersectPacket(packet, packetState);

    for (std::size_t i = 0; i < 4; ++i) {
      State scalarState;
      HitPointInterval hp;
      const bool scalarHit = (bvh->intersect(testRays[i], hp, scalarState) != nullptr);
      ASSERT_TRUE(scalarHit) << "Lane " << i << " expected a hit";
      ASSERT_TRUE(result.hit(i)) << "Lane " << i << " packet expected a hit";

      const double scalarT = hp.minWithPositiveDistance().distance();
      const double packetT = static_cast<double>(result.tNear[i]);
      // Ray4 uses float lanes, so compare within float epsilon.
      EXPECT_NEAR(scalarT, packetT, 1e-3)
        << "Lane " << i << ": scalar t=" << scalarT << " packet t=" << packetT;
    }
  }

  TEST(BVH, PacketIntersectMatchesScalarForCoherentRay4Traversal) {
    auto bvh = gridSpheres(4);

    const std::array<Rayd, 4> coherentRays = {
      Rayd(Vector3d(-10, 0.00, 0.00), Vector3d(1, 0, 0)),
      Rayd(Vector3d(-10, 0.05, 0.00), Vector3d(1, 0, 0)),
      Rayd(Vector3d(-10, 0.00, 0.05), Vector3d(1, 0, 0)),
      Rayd(Vector3d(-10, 0.05, 0.05), Vector3d(1, 0, 0)),
    };

    expectPacketMatchesScalarLanes(*bvh, coherentRays);
  }

  TEST(BVH, PacketIntersectMatchesScalarForIncoherentRay4Traversal) {
    auto bvh = gridSpheres(4);

    const std::array<Rayd, 4> incoherentRays = {
      Rayd(Vector3d(-10, 0, 0), Vector3d(1, 0, 0)),
      Rayd(Vector3d(0, -10, 4), Vector3d(0, 1, 0)),
      Rayd(Vector3d(100, 100, 100), Vector3d(1, 0, 0)),
      Rayd(Vector3d(6, 6, -10), Vector3d(0, 0, 1)),
    };

    expectPacketMatchesScalarLanes(*bvh, incoherentRays);
  }

  TEST(BVH, PacketHitMaterializationMatchesScalarForRay4Traversal) {
    auto bvh = gridSpheres(4);

    const std::array<Rayd, 4> testRays = {
      Rayd(Vector3d(-10, 0, 0), Vector3d(1, 0, 0)),
      Rayd(Vector3d(0, -10, 4), Vector3d(0, 1, 0)),
      Rayd(Vector3d(100, 100, 100), Vector3d(1, 0, 0)),
      Rayd(Vector3d(6, 6, -10), Vector3d(0, 0, 1)),
    };

    expectPacketHitsMatchScalarLanes(*bvh, testRays);
  }

  TEST(BVH, PacketIntersectFallsBackToLinearScanIfSetupNotCalled) {
    BVH bvh;
    auto sphere = std::make_shared<Sphere>(Vector3d::null, 1.0);
    bvh.add(sphere);
    // No setup() call — should fall back to base class scalar loop.

    const std::array<Rayd, 4> testRays = {
      Rayd(Vector3d(0, 0, -10), Vector3d(0, 0, 1)), // hits
      Rayd(Vector3d(5, 0, -10), Vector3d(0, 0, 1)), // misses (x=5, sphere at origin)
      Rayd(Vector3d(0, 0, -10), Vector3d(0, 0, 1)), // hits
      Rayd(Vector3d(5, 0, -10), Vector3d(0, 0, 1)), // misses
    };
    Ray4 packet(testRays);
    State state;
    const auto result = bvh.intersectPacket(packet, state);
    EXPECT_TRUE(result.hit(0));
    EXPECT_FALSE(result.hit(1));
    EXPECT_TRUE(result.hit(2));
    EXPECT_FALSE(result.hit(3));
  }

  TEST(BVH, PacketHitMaterializationFallsBackToLinearScanIfSetupNotCalled) {
    BVH bvh;
    auto sphere = std::make_shared<Sphere>(Vector3d::null, 1.0);
    bvh.add(sphere);

    const std::array<Rayd, 4> testRays = {
      Rayd(Vector3d(0, 0, -10), Vector3d(0, 0, 1)),
      Rayd(Vector3d(5, 0, -10), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, 0, -10), Vector3d(0, 0, 1)),
      Rayd(Vector3d(5, 0, -10), Vector3d(0, 0, 1)),
    };
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};
    const auto result = bvh.intersectPacketHits(Ray4(testRays), states);

    EXPECT_TRUE(result.hit(0));
    EXPECT_EQ(sphere.get(), result.primitive(0));
    EXPECT_FALSE(result.hit(1));
    EXPECT_TRUE(result.hit(2));
    EXPECT_EQ(sphere.get(), result.primitive(2));
    EXPECT_FALSE(result.hit(3));
  }
}
