// Benchmarks for BVH block-batched packet traversal (Epic #141, Phase 4.3).
//
// Compares BVH::intersectPacket(Ray4) against four sequential BVH::intersect
// calls for the same rays. Two ray populations:
//
//   Coherent   — four adjacent primary rays from a 2×2 pixel tile looking at
//                the scene (nearly identical traversal paths; maximises cache
//                reuse and active-mask density). Expected: ≥2.5× vs scalar.
//
//   Incoherent — four random rays from random origins and directions (sparse
//                active masks; traversal paths diverge quickly). Expected:
//                within 20% of scalar — confirms the packet path does not
//                regress random-ray workloads like AO or glossy bounces.
//
// The macro benchmarks (bm_bvh_primary_render_*) simulate a full primary-ray
// pass over a 256×256 virtual image using 2×2 pixel tiles. This is the
// "whole-render ≥15% improvement" gate in the acceptance criteria.
//
// bm_bvh_packet4x2_coherent8 measures the Phase 5 ARM Ray8 policy question:
// an eight-ray coherent tile processed as two explicit Ray4 traversals. Native
// bm_bvh_packet8_coherent remains AVX-only.
//
// Build and run:
//   cmake --preset benchmark && cmake --build --preset benchmark --target benchmarks
//   ./build/benchmark/benchmarks/benchmarks --benchmark_filter=bm_bvh_

#include <benchmark/benchmark.h>

#include "BenchmarkHelpers.h"

#include "core/SimdFeatures.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/util/Bit.h"
#include "render/State.h"
#include "render/primitives/BVH.h"
#include "render/primitives/Triangle.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

namespace {
  using namespace render;

  // Tessellated wall of triangles: 32×32 cells, two triangles per cell.
  // 2048 primitives keep the benchmark BVH-heavy while matching the Phase 4.3
  // acceptance workload of a ~1000+ triangle scene.
  constexpr int kSide = 32;

  std::shared_ptr<BVH> buildScene() {
    auto bvh = std::make_shared<BVH>();
    // Keep enough triangles in each leaf that leaf packet SIMD is visible
    // alongside the shared tree walk; this benchmark measures both together.
    bvh->setLeafSize(192);
    for (int y = 0; y < kSide; ++y) {
      for (int z = 0; z < kSide; ++z) {
        const Vector3d p00(0.0, y, z);
        const Vector3d p10(0.0, y + 1.0, z);
        const Vector3d p01(0.0, y, z + 1.0);
        const Vector3d p11(0.0, y + 1.0, z + 1.0);
        bvh->add(std::make_shared<Triangle>(p00, p10, p11));
        bvh->add(std::make_shared<Triangle>(p00, p11, p01));
      }
    }
    bvh->setup();
    return bvh;
  }

  // ── Coherent ray generation ─────────────────────────────────────────────
  //
  // Eye at (-30, 16, 16) looking at the triangle wall center. For each
  // group, pick a random 2×2 pixel tile from a 2048×2048 virtual image and
  // cast one ray per pixel. Adjacent pixels share nearly the same direction,
  // so all four rays follow essentially the same BVH path — maximising both
  // cache reuse at internal nodes and active-mask density at leaf nodes.

  struct CoherentGroup {
    std::array<Rayd, 4> rays{
      Rayd(Vector3d::null, Vector3d::forward()), Rayd(Vector3d::null, Vector3d::forward()),
      Rayd(Vector3d::null, Vector3d::forward()), Rayd(Vector3d::null, Vector3d::forward())};
  };

  std::vector<CoherentGroup> generateCoherentGroups(int numGroups) {
    const Vector3d eye(-30.0, kSide * 0.5, kSide * 0.5);
    const Vector3d center(0.0, kSide * 0.5, kSide * 0.5);

    const Vector3d fwd = (center - eye).normalized();
    const Vector3d worldUp(0, 1, 0);
    const Vector3d right = fwd.crossProduct(worldUp).normalized();
    const Vector3d up = right.crossProduct(fwd).normalized();

    constexpr int kRes = 2048;
    constexpr double kPixelSize = static_cast<double>(kSide) / kRes;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> tileDist(0, kRes / 2 - 1);

    std::vector<CoherentGroup> groups;
    groups.reserve(numGroups);
    for (int g = 0; g < numGroups; ++g) {
      const int tx = tileDist(rng) * 2;
      const int ty = tileDist(rng) * 2;
      CoherentGroup group;
      int k = 0;
      for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
          const double u = (tx + dx - kRes * 0.5) * kPixelSize;
          const double v = (ty + dy - kRes * 0.5) * kPixelSize;
          group.rays[k++] = Rayd(eye, (fwd + right * u + up * v).normalized());
        }
      }
      groups.push_back(group);
    }
    return groups;
  }

  // ── Incoherent ray generation ───────────────────────────────────────────
  //
  // Four random rays grouped arbitrarily. Traversal paths diverge almost
  // immediately, so the active mask thins out quickly — worst case for
  // packet traversal; used to verify the overhead stays within 20% of
  // doing four separate scalar traversals.

  struct IncoherentGroup {
    std::array<Rayd, 4> rays{
      Rayd(Vector3d::null, Vector3d::forward()), Rayd(Vector3d::null, Vector3d::forward()),
      Rayd(Vector3d::null, Vector3d::forward()), Rayd(Vector3d::null, Vector3d::forward())};
  };

  std::vector<IncoherentGroup> generateIncoherentGroups(int numGroups) {
    std::mt19937 rng(99);
    const double extent = kSide;
    std::uniform_real_distribution<double> origDist(-extent, extent * 2);
    std::uniform_real_distribution<double> dirDist(-1.0, 1.0);

    std::vector<IncoherentGroup> groups;
    groups.reserve(numGroups);
    for (int g = 0; g < numGroups; ++g) {
      IncoherentGroup group;
      for (int i = 0; i < 4; ++i) {
        Vector3d o(origDist(rng), origDist(rng), origDist(rng));
        group.rays[i] = Rayd(o, randomUnitDirection(rng, dirDist));
      }
      groups.push_back(group);
    }
    return groups;
  }

  Ray4 toRay4(const std::array<Rayd, 4>& rays) {
    return Ray4(rays);
  }

  // ── Coherent benchmarks ─────────────────────────────────────────────────

  void bm_bvh_scalar_coherent(benchmark::State& benchState) {
    const auto bvh = buildScene();
    const auto groups = generateCoherentGroups(1024);
    for (auto _ : benchState) {
      int hits = 0;
      for (const auto& group : groups) {
        for (const auto& ray : group.rays) {
          State traceState;
          HitPointInterval hp;
          if (bvh->intersect(ray, hp, traceState))
            ++hits;
          benchmark::DoNotOptimize(hp);
        }
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    benchState.SetItemsProcessed(benchState.iterations() * groups.size() * 4);
  }

  void bm_bvh_packet4_coherent(benchmark::State& benchState) {
    const auto bvh = buildScene();
    const auto groups = generateCoherentGroups(1024);
    for (auto _ : benchState) {
      int hits = 0;
      for (const auto& group : groups) {
        State traceState;
        auto result = bvh->intersectPacket(toRay4(group.rays), traceState);
        hits += core::countSetBits(result.hitMask);
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    benchState.SetItemsProcessed(benchState.iterations() * groups.size() * 4);
  }

  // ── Incoherent benchmarks ───────────────────────────────────────────────

  void bm_bvh_scalar_incoherent(benchmark::State& benchState) {
    const auto bvh = buildScene();
    const auto groups = generateIncoherentGroups(1024);
    for (auto _ : benchState) {
      int hits = 0;
      for (const auto& group : groups) {
        for (const auto& ray : group.rays) {
          State traceState;
          HitPointInterval hp;
          if (bvh->intersect(ray, hp, traceState))
            ++hits;
          benchmark::DoNotOptimize(hp);
        }
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    benchState.SetItemsProcessed(benchState.iterations() * groups.size() * 4);
  }

  void bm_bvh_packet4_incoherent(benchmark::State& benchState) {
    const auto bvh = buildScene();
    const auto groups = generateIncoherentGroups(1024);
    for (auto _ : benchState) {
      int hits = 0;
      for (const auto& group : groups) {
        State traceState;
        auto result = bvh->intersectPacket(toRay4(group.rays), traceState);
        hits += core::countSetBits(result.hitMask);
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    benchState.SetItemsProcessed(benchState.iterations() * groups.size() * 4);
  }

  // ── Macro: simulated primary-ray render pass ────────────────────────────
  //
  // Traces all primary rays for a 256×256 image as 2×2 pixel tiles. The
  // ratio bm_bvh_primary_render_scalar / bm_bvh_primary_render_packet4
  // measures the whole-render traversal improvement (≥15% target).

  void bm_bvh_primary_render_scalar(benchmark::State& benchState) {
    const auto bvh = buildScene();
    const auto groups = generateCoherentGroups(256 * 256 / 4);
    for (auto _ : benchState) {
      int hits = 0;
      for (const auto& group : groups) {
        for (const auto& ray : group.rays) {
          State traceState;
          HitPointInterval hp;
          if (bvh->intersect(ray, hp, traceState))
            ++hits;
          benchmark::DoNotOptimize(hp);
        }
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    benchState.SetItemsProcessed(benchState.iterations() * 256 * 256);
  }

  void bm_bvh_primary_render_packet4(benchmark::State& benchState) {
    const auto bvh = buildScene();
    const auto groups = generateCoherentGroups(256 * 256 / 4);
    for (auto _ : benchState) {
      int hits = 0;
      for (const auto& group : groups) {
        State traceState;
        auto result = bvh->intersectPacket(toRay4(group.rays), traceState);
        hits += core::countSetBits(result.hitMask);
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    benchState.SetItemsProcessed(benchState.iterations() * 256 * 256);
  }

  struct CoherentGroup8 {
    std::array<Rayd, 8> rays{
      Rayd(Vector3d::null, Vector3d::forward()), Rayd(Vector3d::null, Vector3d::forward()),
      Rayd(Vector3d::null, Vector3d::forward()), Rayd(Vector3d::null, Vector3d::forward()),
      Rayd(Vector3d::null, Vector3d::forward()), Rayd(Vector3d::null, Vector3d::forward()),
      Rayd(Vector3d::null, Vector3d::forward()), Rayd(Vector3d::null, Vector3d::forward())};
  };

  std::vector<CoherentGroup8> generateCoherentGroups8(int numGroups) {
    const Vector3d eye(-30.0, kSide * 0.5, kSide * 0.5);
    const Vector3d center(0.0, kSide * 0.5, kSide * 0.5);
    const Vector3d fwd = (center - eye).normalized();
    const Vector3d worldUp(0, 1, 0);
    const Vector3d right = fwd.crossProduct(worldUp).normalized();
    const Vector3d up = right.crossProduct(fwd).normalized();

    constexpr int kRes = 2048;
    constexpr double kPixelSize = static_cast<double>(kSide) / kRes;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> tileDist(0, kRes / 2 - 2);

    std::vector<CoherentGroup8> groups;
    groups.reserve(numGroups);
    for (int g = 0; g < numGroups; ++g) {
      const int tx = tileDist(rng) * 2;
      const int ty = tileDist(rng) * 2;
      CoherentGroup8 group;
      int k = 0;
      // 2×4 tile
      for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 4; ++dx) {
          const double u = (tx + dx - kRes * 0.5) * kPixelSize;
          const double v = (ty + dy - kRes * 0.5) * kPixelSize;
          group.rays[k++] = Rayd(eye, (fwd + right * u + up * v).normalized());
        }
      }
      groups.push_back(group);
    }
    return groups;
  }

  Ray4 toRay4Chunk(const std::array<Rayd, 8>& rays, std::size_t offset) {
    return Ray4(
      std::array<Rayd, 4>{rays[offset], rays[offset + 1], rays[offset + 2], rays[offset + 3]});
  }

  void bm_bvh_packet4x2_coherent8(benchmark::State& benchState) {
    const auto bvh = buildScene();
    const auto groups = generateCoherentGroups8(1024);
    for (auto _ : benchState) {
      int hits = 0;
      for (const auto& group : groups) {
        State firstTraceState;
        const auto first = bvh->intersectPacket(toRay4Chunk(group.rays, 0), firstTraceState);
        hits += core::countSetBits(first.hitMask);

        State secondTraceState;
        const auto second = bvh->intersectPacket(toRay4Chunk(group.rays, 4), secondTraceState);
        hits += core::countSetBits(second.hitMask);
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    benchState.SetItemsProcessed(benchState.iterations() * groups.size() * 8);
  }

  void bm_bvh_scalar_coherent8(benchmark::State& benchState) {
    const auto bvh = buildScene();
    const auto groups = generateCoherentGroups8(1024);
    for (auto _ : benchState) {
      int hits = 0;
      for (const auto& group : groups) {
        for (const auto& ray : group.rays) {
          State traceState;
          HitPointInterval hp;
          if (bvh->intersect(ray, hp, traceState))
            ++hits;
          benchmark::DoNotOptimize(hp);
        }
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    benchState.SetItemsProcessed(benchState.iterations() * groups.size() * 8);
  }

#if RAYTRACER_SIMD_AVX
  // ── Ray8 benchmarks (AVX only) ──────────────────────────────────────────

  Ray8 toRay8(const std::array<Rayd, 8>& rays) {
    return Ray8(rays);
  }

  void bm_bvh_packet8_coherent(benchmark::State& benchState) {
    const auto bvh = buildScene();
    const auto groups = generateCoherentGroups8(1024);
    for (auto _ : benchState) {
      int hits = 0;
      for (const auto& group : groups) {
        State traceState;
        auto result = bvh->intersectPacket(toRay8(group.rays), traceState);
        hits += core::countSetBits(result.hitMask);
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    benchState.SetItemsProcessed(benchState.iterations() * groups.size() * 8);
  }
#endif // RAYTRACER_SIMD_AVX

} // namespace

BENCHMARK(bm_bvh_scalar_coherent);
BENCHMARK(bm_bvh_packet4_coherent);

BENCHMARK(bm_bvh_scalar_incoherent);
BENCHMARK(bm_bvh_packet4_incoherent);

BENCHMARK(bm_bvh_primary_render_scalar);
BENCHMARK(bm_bvh_primary_render_packet4);

BENCHMARK(bm_bvh_scalar_coherent8);
#if RAYTRACER_SIMD_AVX
BENCHMARK(bm_bvh_packet8_coherent);
#endif
BENCHMARK(bm_bvh_packet4x2_coherent8);
