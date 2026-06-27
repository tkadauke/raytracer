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
#include <type_traits>
#include <vector>

namespace BVHPerformanceTest {
  using namespace ::testing;
  using namespace render;

  using Clock = std::chrono::steady_clock;
  constexpr int kMeasuredIterations = 5;

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

  // Deterministic ray batch through the gaps between grid spheres.
  // These miss every primitive, so Composite must scan all leaves while
  // BVH can reject most nodes through bounding boxes.
  std::vector<Rayd> generateMissRays(int count, int side) {
    const int gapCount = std::max(1, side - 1);

    std::vector<Rayd> rays;
    rays.reserve(count);
    for (int i = 0; i < count; ++i) {
      const double y = static_cast<double>((i % gapCount) * 2 + 1);
      const double z = static_cast<double>(((i / gapCount) % gapCount) * 2 + 1);
      rays.emplace_back(Vector3d(-1.0, y, z), Vector3d(1.0, 0.0, 0.0));
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

    auto best = std::chrono::nanoseconds::max();
    for (int iteration = 0; iteration < kMeasuredIterations; ++iteration) {
      const auto start = Clock::now();
      for (const auto& ray : rays) {
        State state;
        HitPointInterval hits;
        container->intersect(ray, hits, state);
      }
      const auto elapsed = Clock::now() - start;
      if (elapsed < best) {
        best = elapsed;
      }
    }
    return best;
  }

  template<class Container>
  std::chrono::nanoseconds timeShadowRay(int side, const std::vector<Rayd>& rays) {
    auto container = buildScene<Container>(side);

    for (const auto& ray : rays) {
      State state;
      container->intersects(ray, state);
    }

    auto best = std::chrono::nanoseconds::max();
    for (int iteration = 0; iteration < kMeasuredIterations; ++iteration) {
      const auto start = Clock::now();
      for (const auto& ray : rays) {
        State state;
        container->intersects(ray, state);
      }
      const auto elapsed = Clock::now() - start;
      if (elapsed < best) {
        best = elapsed;
      }
    }
    return best;
  }

  // Conservative thresholds — observed ratios on dev hardware
  // (release build) are far higher; CI / debug-build margins are
  // baked in. Updates to these thresholds should be either *raising*
  // the floor (stricter perf contract — celebrate) or in a separate
  // PR that explains why the floor is dropping (regression
  // accepted).
  constexpr double kMinIntersectRatioVsComposite = 5.0; // observed ≈37×
  constexpr double kMinShadowRatioVsComposite = 5.0;    // observed ≈113×

  TEST(BVHPerformance, IntersectIsAtLeast5xFasterThanComposite) {
    constexpr int kSide = 8; // 512 primitives
    const auto rays = generateMissRays(256, kSide);

    const auto bvh = timeIntersect<BVH>(kSide, rays);
    const auto comp = timeIntersect<Composite>(kSide, rays);

    const double ratio = static_cast<double>(comp.count()) / bvh.count();
    EXPECT_GT(ratio, kMinIntersectRatioVsComposite)
      << "BVH intersect was only " << ratio << "× faster than Composite "
      << "(BVH=" << bvh.count() << "ns, Composite=" << comp.count() << "ns); "
      << "expected at least " << kMinIntersectRatioVsComposite << "×.";
  }

  TEST(BVHPerformance, ShadowRayIsAtLeast5xFasterThanComposite) {
    constexpr int kSide = 8; // 512 primitives
    const auto rays = generateMissRays(2048, kSide);

    const auto bvh = timeShadowRay<BVH>(kSide, rays);
    const auto comp = timeShadowRay<Composite>(kSide, rays);

    const double ratio = static_cast<double>(comp.count()) / bvh.count();
    EXPECT_GT(ratio, kMinShadowRatioVsComposite)
      << "BVH shadow-ray was only " << ratio << "× faster than Composite "
      << "(BVH=" << bvh.count() << "ns, Composite=" << comp.count() << "ns); "
      << "expected at least " << kMinShadowRatioVsComposite << "×.";
  }

}
