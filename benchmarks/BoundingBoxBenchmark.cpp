// Microbenchmarks for BoundingBox<T>. The intersects(Ray) call is the
// hottest single function in the BVH-accelerated renderer — every internal
// BVH node tests it once per ray. Two batch sizes: 256-ray (fits in L1) and
// 10k-ray (spills to L2/L3, closer to real BVH traversal access patterns).

#include <benchmark/benchmark.h>

#include "core/math/BoundingBox.h"
#include "core/math/Range.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/Vector.h"
#include "core/simd/Float4.h"

#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace {

  template<typename T>
  std::vector<Rayd> generateRays(int count, T extent) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> origin(-double(extent) * 2, double(extent) * 2);
    std::uniform_real_distribution<double> direction(-1.0, 1.0);

    std::vector<Rayd> rays;
    rays.reserve(count);
    for (int i = 0; i < count; ++i) {
      Vector4d o(origin(rng), origin(rng), origin(rng), 1.0);
      Vector3d d(direction(rng), direction(rng), direction(rng));
      if (d.length() < 1e-6)
        d = Vector3d(1, 0, 0);
      rays.emplace_back(o, d.normalized());
    }
    return rays;
  }

  std::vector<Ray4> packetize(const std::vector<Rayd>& rays) {
    std::vector<Ray4> packets;
    packets.reserve(rays.size() / Ray4::lanes);
    for (std::size_t i = 0; i < rays.size(); i += Ray4::lanes) {
      packets.emplace_back(std::array<Rayd, 4>{rays[i], rays[i + 1], rays[i + 2], rays[i + 3]});
    }
    return packets;
  }

  int countBits(std::uint16_t mask) {
    int count = 0;
    while (mask != 0) {
      count += mask & 1u;
      mask >>= 1u;
    }
    return count;
  }

  template<typename T>
  void bm_intersects(benchmark::State& state) {
    BoundingBox<T> box(Vector3<T>(-1, -1, -1), Vector3<T>(1, 1, 1));
    const auto rays = generateRays<T>(256, T(2));
    for (auto _ : state) {
      int hits = 0;
      for (const auto& ray : rays) {
        if (box.intersects(ray))
          ++hits;
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * rays.size());
  }

  // 10k-ray batch — the primary gate for the Phase 1.2 SIMD target (≥3×).
  // Larger than L1 D-cache so it exercises the out-of-order engine under
  // realistic memory pressure.
  template<typename T>
  void bm_intersects_batch(benchmark::State& state) {
    BoundingBox<T> box(Vector3<T>(-1, -1, -1), Vector3<T>(1, 1, 1));
    const auto rays = generateRays<T>(10000, T(2));
    for (auto _ : state) {
      int hits = 0;
      for (const auto& ray : rays) {
        if (box.intersects(ray))
          ++hits;
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * rays.size());
  }

  template<typename T>
  void bm_intersects4_packet_batch(benchmark::State& state) {
    BoundingBox<T> box(Vector3<T>(-1, -1, -1), Vector3<T>(1, 1, 1));
    const auto rays = generateRays<T>(10000, T(2));
    const auto packets = packetize(rays);
    for (auto _ : state) {
      int hits = 0;
      for (const auto& packet : packets) {
        hits +=
          countBits(static_cast<std::uint16_t>(core::simd::movemask(box.intersects4(packet))));
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * rays.size());
  }

  // Batch variant for the new intersect(Ray, Range&) overload that returns
  // the [t_enter, t_exit] interval — lets BVH avoid recomputing entry times.
  template<typename T>
  void bm_intersect_interval(benchmark::State& state) {
    BoundingBox<T> box(Vector3<T>(-1, -1, -1), Vector3<T>(1, 1, 1));
    const auto rays = generateRays<T>(10000, T(2));
    for (auto _ : state) {
      int hits = 0;
      for (const auto& ray : rays) {
        Range<T> interval(T(0), T(0));
        if (box.intersect(ray, interval))
          ++hits;
        benchmark::DoNotOptimize(interval);
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * rays.size());
  }

  template<typename T>
  std::vector<Vector3<T>> inverseDirectionsFor(const std::vector<Rayd>& rays) {
    std::vector<Vector3<T>> inverseDirections;
    inverseDirections.reserve(rays.size());
    for (const auto& ray : rays) {
      inverseDirections.emplace_back(T(1) / T(ray.direction().x()), T(1) / T(ray.direction().y()),
                                     T(1) / T(ray.direction().z()));
    }
    return inverseDirections;
  }

  // Same bool query, but with the ray's reciprocal direction precomputed once.
  // This mirrors BVH traversal, where one ray is tested against many node boxes.
  template<typename T>
  void bm_intersects_precomputed_inverse(benchmark::State& state) {
    BoundingBox<T> box(Vector3<T>(-1, -1, -1), Vector3<T>(1, 1, 1));
    const auto rays = generateRays<T>(10000, T(2));
    const auto inverseDirections = inverseDirectionsFor<T>(rays);
    for (auto _ : state) {
      int hits = 0;
      for (std::size_t i = 0; i != rays.size(); ++i) {
        if (box.intersects(rays[i], inverseDirections[i]))
          ++hits;
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * rays.size());
  }

  template<typename T>
  void bm_intersect_interval_precomputed_inverse(benchmark::State& state) {
    BoundingBox<T> box(Vector3<T>(-1, -1, -1), Vector3<T>(1, 1, 1));
    const auto rays = generateRays<T>(10000, T(2));
    const auto inverseDirections = inverseDirectionsFor<T>(rays);
    for (auto _ : state) {
      int hits = 0;
      for (std::size_t i = 0; i != rays.size(); ++i) {
        Range<T> interval(T(0), T(0));
        if (box.intersect(rays[i], inverseDirections[i], interval))
          ++hits;
        benchmark::DoNotOptimize(interval);
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * rays.size());
  }

  template<typename T>
  void bm_contains_point(benchmark::State& state) {
    BoundingBox<T> box(Vector3<T>(-1, -1, -1), Vector3<T>(1, 1, 1));
    std::vector<Vector3<T>> points;
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> u(-2.0, 2.0);
    for (int i = 0; i < 256; ++i) {
      points.emplace_back(T(u(rng)), T(u(rng)), T(u(rng)));
    }
    for (auto _ : state) {
      int hits = 0;
      for (const auto& p : points) {
        if (box.contains(p))
          ++hits;
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * points.size());
  }

  template<typename T>
  void bm_union(benchmark::State& state) {
    BoundingBox<T> a(Vector3<T>(-1, -1, -1), Vector3<T>(1, 1, 1));
    BoundingBox<T> b(Vector3<T>(0, 0, 0), Vector3<T>(2, 2, 2));
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      benchmark::DoNotOptimize(b);
      auto r = a | b;
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_intersection(benchmark::State& state) {
    BoundingBox<T> a(Vector3<T>(-1, -1, -1), Vector3<T>(1, 1, 1));
    BoundingBox<T> b(Vector3<T>(0, 0, 0), Vector3<T>(2, 2, 2));
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      benchmark::DoNotOptimize(b);
      auto r = a & b;
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_volume(benchmark::State& state) {
    BoundingBox<T> a(Vector3<T>(-1, -1, -1), Vector3<T>(1, 1, 1));
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      auto r = a.volume();
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_include_point(benchmark::State& state) {
    std::vector<Vector3<T>> points;
    std::mt19937 rng(11);
    std::uniform_real_distribution<double> u(-1.0, 1.0);
    for (int i = 0; i < 256; ++i) {
      points.emplace_back(T(u(rng)), T(u(rng)), T(u(rng)));
    }
    for (auto _ : state) {
      BoundingBox<T> box;
      for (const auto& p : points)
        box.include(p);
      benchmark::DoNotOptimize(box);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * points.size());
  }

} // namespace

BENCHMARK(bm_intersects<float>);
BENCHMARK(bm_intersects<double>);

BENCHMARK(bm_intersects_batch<float>);
BENCHMARK(bm_intersects_batch<double>);

BENCHMARK(bm_intersects4_packet_batch<float>);
BENCHMARK(bm_intersects4_packet_batch<double>);

BENCHMARK(bm_intersect_interval<float>);
BENCHMARK(bm_intersect_interval<double>);

BENCHMARK(bm_intersects_precomputed_inverse<float>);
BENCHMARK(bm_intersects_precomputed_inverse<double>);

BENCHMARK(bm_intersect_interval_precomputed_inverse<float>);
BENCHMARK(bm_intersect_interval_precomputed_inverse<double>);

BENCHMARK(bm_contains_point<float>);
BENCHMARK(bm_contains_point<double>);

BENCHMARK(bm_union<float>);
BENCHMARK(bm_union<double>);

BENCHMARK(bm_intersection<float>);
BENCHMARK(bm_intersection<double>);

BENCHMARK(bm_volume<float>);
BENCHMARK(bm_volume<double>);

BENCHMARK(bm_include_point<float>);
BENCHMARK(bm_include_point<double>);
