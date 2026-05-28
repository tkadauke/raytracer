// Microbenchmarks for the polynomial root-finders used by the renderer's
// quadric, cubic, and quartic intersection paths. Quartic::solve() is the
// per-ray cost of torus intersection, including the inner Cubic solve and
// up to two Quadric solves. sortedResult() returns a fixed-capacity result
// object; this benchmark keeps that zero-allocation path covered.

#include <benchmark/benchmark.h>

#include "core/math/Cubic.h"
#include "core/math/Polynomial.h"
#include "core/math/Quadric.h"
#include "core/math/Quartic.h"

#include <array>

namespace {

  // Coefficients chosen so each solver hits its "common case" branch:
  // quadric with two real roots, cubic with three real roots, quartic with
  // four real roots. Stresses the full execution path of each solver.
  template<typename T>
  void bm_quadric_solve(benchmark::State& state) {
    T a = T(1), b = T(-3), c = T(2); // roots 1, 2
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      benchmark::DoNotOptimize(b);
      benchmark::DoNotOptimize(c);
      Quadric<T> q(a, b, c);
      int n = q.solve();
      benchmark::DoNotOptimize(n);
      benchmark::DoNotOptimize(q);
    }
  }

  template<typename T>
  void bm_cubic_solve(benchmark::State& state) {
    T a = T(1), b = T(-6), c = T(11), d = T(-6); // roots 1, 2, 3
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      benchmark::DoNotOptimize(b);
      benchmark::DoNotOptimize(c);
      benchmark::DoNotOptimize(d);
      Cubic<T> p(a, b, c, d);
      int n = p.solve();
      benchmark::DoNotOptimize(n);
      benchmark::DoNotOptimize(p);
    }
  }

  template<typename T>
  void bm_quartic_solve(benchmark::State& state) {
    T a = T(1), b = T(-10), c = T(35), d = T(-50), e = T(24); // roots 1, 2, 3, 4
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      benchmark::DoNotOptimize(b);
      benchmark::DoNotOptimize(c);
      benchmark::DoNotOptimize(d);
      benchmark::DoNotOptimize(e);
      Quartic<T> q(a, b, c, d, e);
      int n = q.solve();
      benchmark::DoNotOptimize(n);
      benchmark::DoNotOptimize(q);
    }
  }

  // solve() variant followed by sortedResult(), matching the torus
  // intersection path while keeping the roots in fixed-capacity storage.
  template<typename T>
  void bm_quartic_sorted_result(benchmark::State& state) {
    T a = T(1), b = T(-10), c = T(35), d = T(-50), e = T(24);
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      benchmark::DoNotOptimize(b);
      benchmark::DoNotOptimize(c);
      benchmark::DoNotOptimize(d);
      benchmark::DoNotOptimize(e);
      Quartic<T> q(a, b, c, d, e);
      auto res = q.sortedResult();
      benchmark::DoNotOptimize(res);
      benchmark::ClobberMemory();
    }
  }

  // solveInto() into a stack array — the lowest-cost solve API today.
  // Confirms whether the std::vector in sortedResult is the only cost
  // or whether there's also dispatch overhead.
  template<typename T>
  void bm_quartic_solve_into(benchmark::State& state) {
    T a = T(1), b = T(-10), c = T(35), d = T(-50), e = T(24);
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      benchmark::DoNotOptimize(b);
      benchmark::DoNotOptimize(c);
      benchmark::DoNotOptimize(d);
      benchmark::DoNotOptimize(e);
      Quartic<T> q(a, b, c, d, e);
      std::array<T, 4> result;
      int n = q.solveInto(result.data());
      benchmark::DoNotOptimize(n);
      benchmark::DoNotOptimize(result);
    }
  }

  // Grazing-incidence torus case: tiny leading coefficient on the quartic
  // produces an ill-conditioned solve. Measures stability/throughput
  // under the pathological-but-real path.
  template<typename T>
  void bm_quartic_grazing(benchmark::State& state) {
    T a = T(1e-6), b = T(1), c = T(-2), d = T(1), e = T(-1);
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      benchmark::DoNotOptimize(b);
      benchmark::DoNotOptimize(c);
      benchmark::DoNotOptimize(d);
      benchmark::DoNotOptimize(e);
      Quartic<T> q(a, b, c, d, e);
      int n = q.solve();
      benchmark::DoNotOptimize(n);
      benchmark::DoNotOptimize(q);
    }
  }

} // namespace

BENCHMARK(bm_quadric_solve<float>);
BENCHMARK(bm_quadric_solve<double>);

BENCHMARK(bm_cubic_solve<float>);
BENCHMARK(bm_cubic_solve<double>);

BENCHMARK(bm_quartic_solve<float>);
BENCHMARK(bm_quartic_solve<double>);

BENCHMARK(bm_quartic_sorted_result<float>);
BENCHMARK(bm_quartic_sorted_result<double>);

BENCHMARK(bm_quartic_solve_into<float>);
BENCHMARK(bm_quartic_solve_into<double>);

BENCHMARK(bm_quartic_grazing<float>);
BENCHMARK(bm_quartic_grazing<double>);
