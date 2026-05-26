// Performance-regression tests for `render::BVH`.
//
// These tests assert *ratios*, not absolute timings — the absolute
// numbers vary 10× between debug and release builds and again between
// dev hardware and CI runners, but the ratios between Composite linear
// scan and the BVH SAH tree hold across all of those environments because
// the primitive-intersection cost dominates similarly in each case.
//
// What this catches: a regression that makes BVH fall back to a
// linear-scan equivalent (e.g. someone rips out the AABB cull in
// `intersectNode`, or `setup()` silently no-ops). The test thresholds
// are well below the observed ratios on dev hardware (Apple Silicon,
// release build): intersect ≈37× and shadow ≈113× faster than
// Composite at 512 primitives. The thresholds use a fraction of those
// margins so noisy CI runners don't flap.
//
// What this doesn't catch: subtle slowdowns within the BVH that don't
// kill the AABB cull (e.g. a 2× regression from a worse SAH split
// strategy). For that, run benchmarks/SpatialIndexBenchmark.cpp via
// the benchmark preset and compare the precise numbers.

#include <gtest/gtest.h>

#include "core/math/HitPointInterval.h"
#include "render/State.h"
#include "render/primitives/BVH.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Sphere.h"

#include <chrono>
#include <memory>
#include <random>
#include <type_traits>
#include <vector>

namespace BVHPerformanceTest {
  using namespace ::testing;
  using namespace render;

  using Clock = std::chrono::steady_clock;

  // 3D-grid scene of spheres: side³ primitives, spacing 2.0,
  // radius 0.5. Identical layout to the Google Benchmark in
  // benchmarks/SpatialIndexBenchmark.cpp so numbers stay comparable.
  template<class Container>
  std::shared_ptr<Container> buildScene(int side) {
    auto container = std::make_shared<Container>();
    for (int x = 0; x < side; ++x) {
      for (int y = 0; y < side; ++y) {
        for (int z = 0; z < side; ++z) {
          container->add(std::make_shared<Sphere>(Vector3d(x * 2.0, y * 2.0, z * 2.0), 0.5));
        }
      }
    }
    if constexpr (std::is_same_v<Container, BVH>) {
      container->setup();
    }
    return container;
  }

  // Deterministic ray batch covering the scene volume — same RNG
  // seed across runs so the comparison is apples-to-apples.
  std::vector<Rayd> generateRays(int count, int side) {
    std::mt19937 rng(42);
    const double extent = side * 2.0;
    std::uniform_real_distribution<double> origin(-extent, extent * 2);
    std::uniform_real_distribution<double> direction(-1.0, 1.0);

    std::vector<Rayd> rays;
    rays.reserve(count);
    for (int i = 0; i < count; ++i) {
      Vector3d o(origin(rng), origin(rng), origin(rng));
      Vector3d d(direction(rng), direction(rng), direction(rng));
      if (d.length() < 1e-6)
        d = Vector3d(1, 0, 0);
      rays.emplace_back(o, d.normalized());
    }
    return rays;
  }

  template<class Container>
  std::chrono::nanoseconds timeIntersect(int side, const std::vector<Rayd>& rays) {
    auto container = buildScene<Container>(side);

    // Warm-up pass — flush instruction cache, page in the tree
    // memory, give the branch predictor a chance to settle. Not
    // strictly necessary for the ratio assertion to hold, but reduces
    // noise on the first measured iteration.
    for (const auto& ray : rays) {
      State state;
      HitPointInterval hits;
      container->intersect(ray, hits, state);
    }

    const auto start = Clock::now();
    for (const auto& ray : rays) {
      State state;
      HitPointInterval hits;
      container->intersect(ray, hits, state);
    }
    return Clock::now() - start;
  }

  template<class Container>
  std::chrono::nanoseconds timeShadowRay(int side, const std::vector<Rayd>& rays) {
    auto container = buildScene<Container>(side);

    for (const auto& ray : rays) {
      State state;
      container->intersects(ray, state);
    }

    const auto start = Clock::now();
    for (const auto& ray : rays) {
      State state;
      container->intersects(ray, state);
    }
    return Clock::now() - start;
  }

  // Conservative thresholds — observed ratios on dev hardware
  // (release build) are far higher; CI / debug-build margins are
  // baked in. Updates to these thresholds should be either *raising*
  // the floor (stricter perf contract — celebrate) or in a separate
  // PR that explains why the floor is dropping (regression
  // accepted).
  constexpr double kMinIntersectRatioVsComposite = 5.0; // observed ≈37×
  constexpr double kMinShadowRatioVsComposite = 10.0;   // observed ≈113×

  TEST(BVHPerformance, IntersectIsAtLeast5xFasterThanComposite) {
    constexpr int kSide = 8; // 512 primitives
    const auto rays = generateRays(256, kSide);

    const auto bvh = timeIntersect<BVH>(kSide, rays);
    const auto comp = timeIntersect<Composite>(kSide, rays);

    const double ratio = static_cast<double>(comp.count()) / bvh.count();
    EXPECT_GT(ratio, kMinIntersectRatioVsComposite)
      << "BVH intersect was only " << ratio << "× faster than Composite "
      << "(BVH=" << bvh.count() << "ns, Composite=" << comp.count() << "ns); "
      << "expected at least " << kMinIntersectRatioVsComposite << "×.";
  }

  TEST(BVHPerformance, ShadowRayIsAtLeast10xFasterThanComposite) {
    constexpr int kSide = 8; // 512 primitives
    const auto rays = generateRays(256, kSide);

    const auto bvh = timeShadowRay<BVH>(kSide, rays);
    const auto comp = timeShadowRay<Composite>(kSide, rays);

    const double ratio = static_cast<double>(comp.count()) / bvh.count();
    EXPECT_GT(ratio, kMinShadowRatioVsComposite)
      << "BVH shadow-ray was only " << ratio << "× faster than Composite "
      << "(BVH=" << bvh.count() << "ns, Composite=" << comp.count() << "ns); "
      << "expected at least " << kMinShadowRatioVsComposite << "×.";
  }

}
