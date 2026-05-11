// Microbenchmarks for the Vector hot path. Both the generic and SSE3
// specializations resolve through the same template surface — Vector3<float>
// and Vector4<float> hit the SSE3 paths from include/core/math/vector/sse3/,
// while Vector3<double> and Vector4<double> use different paths (Vector3d is
// the generic template after the Phase-2.3 deletion; Vector4d has its own SSE3
// path). Comparing types side by side gives a quick proxy for whether the SSE3
// inlining still works after a refactor.

#include <benchmark/benchmark.h>

#include "core/math/Vector.h"

#include <vector>

namespace {

// DoNotOptimize on the operands before each iteration is what prevents the
// compiler from folding compile-time-constant inputs down to a precomputed
// result. Without it, every single-op microbenchmark below reports ~1 cycle.

template <typename Vec>
void bm_dot(benchmark::State& state) {
  Vec a(1, 2, 3);
  Vec b(4, 5, 6);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(b);
    auto r = a * b;
    benchmark::DoNotOptimize(r);
  }
}

template <typename Vec>
void bm_add(benchmark::State& state) {
  Vec a(1, 2, 3);
  Vec b(4, 5, 6);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(b);
    auto r = a + b;
    benchmark::DoNotOptimize(r);
  }
}

template <typename Vec>
void bm_sub(benchmark::State& state) {
  Vec a(1, 2, 3);
  Vec b(4, 5, 6);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(b);
    auto r = a - b;
    benchmark::DoNotOptimize(r);
  }
}

template <typename Vec>
void bm_scalar_mul(benchmark::State& state) {
  Vec a(1, 2, 3);
  typename Vec::Coordinate s(0.5);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(s);
    auto r = a * s;
    benchmark::DoNotOptimize(r);
  }
}

template <typename Vec>
void bm_scalar_div(benchmark::State& state) {
  Vec a(1, 2, 3);
  typename Vec::Coordinate s(2);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(s);
    auto r = a / s;
    benchmark::DoNotOptimize(r);
  }
}

template <typename Vec>
void bm_length(benchmark::State& state) {
  Vec a(1, 2, 3);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    auto r = a.length();
    benchmark::DoNotOptimize(r);
  }
}

template <typename Vec>
void bm_squared_length(benchmark::State& state) {
  Vec a(1, 2, 3);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    auto r = a.squaredLength();
    benchmark::DoNotOptimize(r);
  }
}

template <typename Vec>
void bm_normalize(benchmark::State& state) {
  Vec a(1, 2, 3);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    auto r = a.normalized();
    benchmark::DoNotOptimize(r);
  }
}

template <typename Vec>
void bm_cross(benchmark::State& state) {
  Vec a(1, 2, 3);
  Vec b(4, 5, 6);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(b);
    auto r = a.crossProduct(b);
    benchmark::DoNotOptimize(r);
  }
}

// Chained pipeline that mirrors a realistic shading-evaluation step:
// reflect direction = i - 2*(i*n)*n. This catches autovectorization /
// register-allocation behavior that single-op microbenchmarks miss.
template <typename Vec>
void bm_reflect_chain(benchmark::State& state) {
  Vec i(1, -1, 0);
  Vec n(0, 1, 0);
  for (auto _ : state) {
    benchmark::DoNotOptimize(i);
    benchmark::DoNotOptimize(n);
    auto d = i * n;
    auto r = i - n * (typename Vec::Coordinate(2) * d);
    benchmark::DoNotOptimize(r);
  }
}

// reflect() method — same math as bm_reflect_chain but routed through
// the Vector::reflect() helper so we can confirm zero overhead vs the
// open-coded version above.
template <typename Vec>
void bm_reflect(benchmark::State& state) {
  Vec i(1, -1, 0);
  Vec n(0, 1, 0);
  for (auto _ : state) {
    benchmark::DoNotOptimize(i);
    benchmark::DoNotOptimize(n);
    auto r = i.reflect(n);
    benchmark::DoNotOptimize(r);
  }
}

// refract() method — Snell's-law transmitted direction, the other hot
// path in transparent-material shading.
template <typename Vec>
void bm_refract(benchmark::State& state) {
  Vec out(0, 1, 0);
  Vec n(0, 1, 0);
  typename Vec::Coordinate eta(1.5);
  for (auto _ : state) {
    benchmark::DoNotOptimize(out);
    benchmark::DoNotOptimize(n);
    benchmark::DoNotOptimize(eta);
    auto r = out.refract(n, eta);
    benchmark::DoNotOptimize(r);
  }
}

template <typename Vec>
void bm_lerp(benchmark::State& state) {
  Vec a(1, 2, 3);
  Vec b(4, 5, 6);
  typename Vec::Coordinate t(0.3);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(b);
    benchmark::DoNotOptimize(t);
    auto r = a.lerp(b, t);
    benchmark::DoNotOptimize(r);
  }
}

template <typename Vec>
void bm_clamp(benchmark::State& state) {
  Vec a(1, -2, 3);
  typename Vec::Coordinate lo(0), hi(2);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(lo);
    benchmark::DoNotOptimize(hi);
    auto r = a.clamp(lo, hi);
    benchmark::DoNotOptimize(r);
  }
}

template <typename Vec>
void bm_saturate(benchmark::State& state) {
  Vec a(0.5, -0.1, 1.2);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    auto r = a.saturate();
    benchmark::DoNotOptimize(r);
  }
}

template <typename Vec>
void bm_cwise_min(benchmark::State& state) {
  Vec a(1, 5, 3);
  Vec b(4, 2, 6);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(b);
    auto r = a.cwiseMin(b);
    benchmark::DoNotOptimize(r);
  }
}

template <typename Vec>
void bm_cwise_max(benchmark::State& state) {
  Vec a(1, 5, 3);
  Vec b(4, 2, 6);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(b);
    auto r = a.cwiseMax(b);
    benchmark::DoNotOptimize(r);
  }
}

// Batched dot-product over an aligned array. Stresses memory-bandwidth
// behavior and gives the compiler a clearer auto-vectorization target than
// the single-pair benchmarks above.
template <typename Vec>
void bm_dot_batch(benchmark::State& state) {
  constexpr int N = 1024;
  std::vector<Vec> as(N), bs(N);
  for (int k = 0; k < N; ++k) {
    as[k] = Vec(typename Vec::Coordinate(k),
                typename Vec::Coordinate(k + 1),
                typename Vec::Coordinate(k + 2));
    bs[k] = Vec(typename Vec::Coordinate(N - k),
                typename Vec::Coordinate(N - k - 1),
                typename Vec::Coordinate(N - k - 2));
  }
  for (auto _ : state) {
    typename Vec::Coordinate acc{};
    for (int k = 0; k < N; ++k) {
      acc += as[k] * bs[k];
    }
    benchmark::DoNotOptimize(acc);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * N);
}

}  // namespace

#define REGISTER_FOR(VecType) \
  BENCHMARK(bm_dot<VecType>); \
  BENCHMARK(bm_add<VecType>); \
  BENCHMARK(bm_sub<VecType>); \
  BENCHMARK(bm_scalar_mul<VecType>); \
  BENCHMARK(bm_scalar_div<VecType>); \
  BENCHMARK(bm_length<VecType>); \
  BENCHMARK(bm_squared_length<VecType>); \
  BENCHMARK(bm_normalize<VecType>); \
  BENCHMARK(bm_reflect_chain<VecType>); \
  BENCHMARK(bm_reflect<VecType>); \
  BENCHMARK(bm_refract<VecType>); \
  BENCHMARK(bm_lerp<VecType>); \
  BENCHMARK(bm_clamp<VecType>); \
  BENCHMARK(bm_saturate<VecType>); \
  BENCHMARK(bm_cwise_min<VecType>); \
  BENCHMARK(bm_cwise_max<VecType>); \
  BENCHMARK(bm_dot_batch<VecType>);

REGISTER_FOR(Vector3f)
REGISTER_FOR(Vector3d)
REGISTER_FOR(Vector4f)
REGISTER_FOR(Vector4d)

BENCHMARK(bm_cross<Vector3f>);
BENCHMARK(bm_cross<Vector3d>);

#undef REGISTER_FOR
