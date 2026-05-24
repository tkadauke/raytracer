// Microbenchmarks for HitPointInterval. Every primitive::intersect() call
// in the renderer constructs an interval, pushes one or more hit points
// into it, and reads them back out. The current implementation backs the
// interval with std::vector<HitPointWrapper>, which heap-allocates the
// first time anything is pushed — a per-ray allocation on the hot path.
// The 1- and 2-hit cases dominate frequency in real scenes (sphere,
// plane, triangle); higher counts are CSG and torus territory.

#include <benchmark/benchmark.h>

#include "core/math/HitPoint.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Vector.h"

namespace {

  HitPoint makeHitPoint(double distance) {
    return HitPoint(nullptr, distance, Vector4d(distance, 0, 0, 1), Vector3d(0, 1, 0));
  }

  template<int N>
  void bm_push_n(benchmark::State& state) {
    for (auto _ : state) {
      HitPointInterval interval;
      for (int i = 0; i < N; ++i) {
        // alternating in/out matches CSG-shaped intervals.
        interval.add(makeHitPoint(double(i + 1)), (i & 1) == 0);
      }
      benchmark::DoNotOptimize(interval);
      benchmark::ClobberMemory();
    }
  }

  // "Single hit then query" — the dominant primitive case. Constructor,
  // one push, one min() read, destruction.
  void bm_single_hit_cycle(benchmark::State& state) {
    for (auto _ : state) {
      HitPointInterval interval;
      interval.addIn(makeHitPoint(1.5));
      interval.addOut(makeHitPoint(2.5));
      auto mn = interval.min();
      benchmark::DoNotOptimize(mn);
      benchmark::ClobberMemory();
    }
  }

  // Merge of two intervals via operator| — CSG union path.
  void bm_merge_union(benchmark::State& state) {
    HitPointInterval a;
    a.addIn(makeHitPoint(1.0));
    a.addOut(makeHitPoint(2.0));
    HitPointInterval b;
    b.addIn(makeHitPoint(1.5));
    b.addOut(makeHitPoint(2.5));
    for (auto _ : state) {
      auto r = a | b;
      benchmark::DoNotOptimize(r);
      benchmark::ClobberMemory();
    }
  }

  // Merge via operator& — CSG intersection path.
  void bm_merge_intersect(benchmark::State& state) {
    HitPointInterval a;
    a.addIn(makeHitPoint(1.0));
    a.addOut(makeHitPoint(2.0));
    HitPointInterval b;
    b.addIn(makeHitPoint(1.5));
    b.addOut(makeHitPoint(2.5));
    for (auto _ : state) {
      auto r = a & b;
      benchmark::DoNotOptimize(r);
      benchmark::ClobberMemory();
    }
  }

  // Iterate through an N-hit interval. After the SBO refactor we want this
  // to stay flat — the points are now stored inline, but iteration cost
  // should be roughly equivalent.
  void bm_iterate(benchmark::State& state) {
    HitPointInterval interval;
    for (int i = 0; i < 8; ++i) {
      interval.add(makeHitPoint(double(i + 1)), (i & 1) == 0);
    }
    for (auto _ : state) {
      double acc = 0;
      for (const auto& hp : interval.points()) {
        acc += hp.point.distance();
      }
      benchmark::DoNotOptimize(acc);
    }
  }

} // namespace

BENCHMARK(bm_push_n<1>);
BENCHMARK(bm_push_n<2>);
BENCHMARK(bm_push_n<4>);
BENCHMARK(bm_push_n<8>);
BENCHMARK(bm_push_n<16>);

BENCHMARK(bm_single_hit_cycle);
BENCHMARK(bm_merge_union);
BENCHMARK(bm_merge_intersect);
BENCHMARK(bm_iterate);
