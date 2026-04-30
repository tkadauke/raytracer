#include <benchmark/benchmark.h>

#include "core/math/Vector.h"

// Starter microbenchmarks for the Vector hot path. Both the generic and SSE3
// specializations resolve through the same template surface — Vector3<float>
// and Vector4<float> hit the SSE3 paths from include/core/math/vector/sse3/,
// Vector3<double> hits the generic path on x86 (and the SSE2-via-double-pair
// path on Apple Silicon). Comparing the two gives a quick proxy for whether
// the SSE3 inlining still works after a refactor (modernize.md §3.4).

namespace {

template <typename Vec>
void bm_dot(benchmark::State& state) {
  Vec a(1, 2, 3);
  Vec b(4, 5, 6);
  for (auto _ : state) {
    auto r = a * b;
    benchmark::DoNotOptimize(r);
    benchmark::ClobberMemory();
  }
}

template <typename Vec>
void bm_add(benchmark::State& state) {
  Vec a(1, 2, 3);
  Vec b(4, 5, 6);
  for (auto _ : state) {
    auto r = a + b;
    benchmark::DoNotOptimize(r);
    benchmark::ClobberMemory();
  }
}

template <typename Vec>
void bm_length(benchmark::State& state) {
  Vec a(1, 2, 3);
  for (auto _ : state) {
    auto r = a.length();
    benchmark::DoNotOptimize(r);
  }
}

}  // namespace

BENCHMARK(bm_dot<Vector3f>);
BENCHMARK(bm_dot<Vector3d>);
BENCHMARK(bm_add<Vector3f>);
BENCHMARK(bm_add<Vector3d>);
BENCHMARK(bm_length<Vector3f>);
BENCHMARK(bm_length<Vector3d>);
