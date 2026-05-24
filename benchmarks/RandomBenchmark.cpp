// Microbenchmarks for the random-number plumbing. Today's Number::random<T>
// uses std::rand, which holds a global lock and has known-poor statistical
// properties. These benchmarks pin the single-thread throughput and the
// multi-thread contention baseline so a PRNG replacement can prove the
// expected improvements.

#include <benchmark/benchmark.h>

#include "core/math/Number.h"

namespace {

  template<typename T>
  void bm_random_unit(benchmark::State& state) {
    for (auto _ : state) {
      T r = random(T(0), T(1));
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_random_range(benchmark::State& state) {
    for (auto _ : state) {
      T r = random(T(-100), T(100));
      benchmark::DoNotOptimize(r);
    }
  }

  void bm_random_int(benchmark::State& state) {
    for (auto _ : state) {
      int r = random(1024);
      benchmark::DoNotOptimize(r);
    }
  }

  // Batch generation — most renderer code consumes random numbers in tight
  // loops (sampler stratification, jitter, hemisphere sampling). Batches are
  // a closer proxy for the renderer hot path than the single-call variants.
  template<typename T>
  void bm_random_batch(benchmark::State& state) {
    constexpr int N = 1024;
    for (auto _ : state) {
      T acc{};
      for (int i = 0; i < N; ++i) {
        acc += random(T(0), T(1));
      }
      benchmark::DoNotOptimize(acc);
    }
    state.SetItemsProcessed(state.iterations() * N);
  }

  // Multi-thread call. Today std::rand is non-reentrant on glibc and
  // guarded by a global lock on macOS — expect severe contention. Captures
  // the baseline so the per-thread PRNG replacement can show the expected
  // near-linear scaling.
  void bm_random_threaded(benchmark::State& state) {
    for (auto _ : state) {
      double r = random(0.0, 1.0);
      benchmark::DoNotOptimize(r);
    }
  }

} // namespace

BENCHMARK(bm_random_unit<float>);
BENCHMARK(bm_random_unit<double>);
BENCHMARK(bm_random_range<float>);
BENCHMARK(bm_random_range<double>);
BENCHMARK(bm_random_int);
BENCHMARK(bm_random_batch<float>);
BENCHMARK(bm_random_batch<double>);

BENCHMARK(bm_random_threaded)->Threads(1)->Threads(2)->Threads(4)->Threads(8);
