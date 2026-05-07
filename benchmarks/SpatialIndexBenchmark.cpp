// Compares the three Composite-derived containers under the canonical
// "many primitives, many rays" workload that the spatial accelerators
// exist to optimise:
//
//   - Composite — linear scan over every child per ray. The baseline.
//   - Grid      — uniform-cell DDA traversal.
//   - BVH       — Surface Area Heuristic binary tree.
//
// Each benchmark builds the chosen container with N spheres on a 3D
// grid then traces a precomputed batch of rays through it. The
// per-ray cost is what matters for renderer throughput.

#include <benchmark/benchmark.h>

#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "render/State.h"
#include "render/primitives/BVH.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Grid.h"
#include "render/primitives/Sphere.h"

#include <memory>
#include <random>
#include <vector>

namespace {
  using namespace render;

  // A 3D-grid scene of spheres: side³ primitives. Spacing 2.0 with
  // radius 0.5 leaves clear gaps between spheres so most rays miss
  // most spheres — the realistic case for spatial-acceleration wins.
  template<class Container>
  std::shared_ptr<Container> buildScene(int side) {
    auto container = std::make_shared<Container>();
    for (int x = 0; x < side; ++x) {
      for (int y = 0; y < side; ++y) {
        for (int z = 0; z < side; ++z) {
          container->add(std::make_shared<Sphere>(
            Vector3d(x * 2.0, y * 2.0, z * 2.0), 0.5));
        }
      }
    }
    return container;
  }

  // Generate a deterministic batch of rays with mixed origins and
  // directions covering the scene volume — some hit, some miss.
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
      if (d.length() < 1e-6) d = Vector3d(1, 0, 0);
      rays.emplace_back(o, d.normalized());
    }
    return rays;
  }

  template<class Container>
  void bm_intersect(benchmark::State& state) {
    const int side = static_cast<int>(state.range(0));
    auto container = buildScene<Container>(side);
    if constexpr (std::is_same_v<Container, BVH> || std::is_same_v<Container, Grid>) {
      container->setup();
    }

    const auto rays = generateRays(256, side);

    for (auto _ : state) {
      for (const auto& ray : rays) {
        ::render::State traceState;
        HitPointInterval hits;
        auto hit = container->intersect(ray, hits, traceState);
        benchmark::DoNotOptimize(hit);
      }
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * rays.size());
  }

  template<class Container>
  void bm_shadowRay(benchmark::State& state) {
    const int side = static_cast<int>(state.range(0));
    auto container = buildScene<Container>(side);
    if constexpr (std::is_same_v<Container, BVH> || std::is_same_v<Container, Grid>) {
      container->setup();
    }

    const auto rays = generateRays(256, side);

    for (auto _ : state) {
      for (const auto& ray : rays) {
        ::render::State traceState;
        bool hit = container->intersects(ray, traceState);
        benchmark::DoNotOptimize(hit);
      }
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * rays.size());
  }

  template<class Container>
  void bm_build(benchmark::State& state) {
    const int side = static_cast<int>(state.range(0));
    for (auto _ : state) {
      auto container = buildScene<Container>(side);
      if constexpr (std::is_same_v<Container, BVH> || std::is_same_v<Container, Grid>) {
        container->setup();
      }
      benchmark::DoNotOptimize(container);
    }
  }
}

// 3-, 5-, 8-, 12-side cubes → 27, 125, 512, 1728 spheres.
BENCHMARK(bm_intersect<Composite>)->Arg(3)->Arg(5)->Arg(8)->Arg(12);
BENCHMARK(bm_intersect<Grid>)     ->Arg(3)->Arg(5)->Arg(8)->Arg(12);
BENCHMARK(bm_intersect<BVH>)      ->Arg(3)->Arg(5)->Arg(8)->Arg(12);

BENCHMARK(bm_shadowRay<Composite>)->Arg(3)->Arg(5)->Arg(8)->Arg(12);
BENCHMARK(bm_shadowRay<Grid>)     ->Arg(3)->Arg(5)->Arg(8)->Arg(12);
BENCHMARK(bm_shadowRay<BVH>)      ->Arg(3)->Arg(5)->Arg(8)->Arg(12);

BENCHMARK(bm_build<Composite>)->Arg(3)->Arg(5)->Arg(8)->Arg(12);
BENCHMARK(bm_build<Grid>)     ->Arg(3)->Arg(5)->Arg(8)->Arg(12);
BENCHMARK(bm_build<BVH>)      ->Arg(3)->Arg(5)->Arg(8)->Arg(12);
