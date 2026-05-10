// Microbenchmarks for Quaternion. The class is currently minimal — these
// benchmarks pin the throughput of what exists so the planned expansion
// (SLERP, axis-angle, rotate-vector, etc.) doesn't silently regress the
// existing operations.

#include <benchmark/benchmark.h>

#include <iostream>

#include "core/math/Quaternion.h"

namespace {

template <typename T>
void bm_multiply(benchmark::State& state) {
  Quaternion<T> a(T(0.7071), T(0.7071), T(0), T(0));
  Quaternion<T> b(T(0.5), T(0), T(0.866), T(0));
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(b);
    auto r = a * b;
    benchmark::DoNotOptimize(r);
  }
}

template <typename T>
void bm_length(benchmark::State& state) {
  Quaternion<T> q(T(0.7071), T(0.7071), T(0), T(0));
  for (auto _ : state) {
    benchmark::DoNotOptimize(q);
    T r = q.length();
    benchmark::DoNotOptimize(r);
  }
}

template <typename T>
void bm_normalize(benchmark::State& state) {
  Quaternion<T> q(T(0.5), T(0.5), T(0.5), T(0.5));
  for (auto _ : state) {
    benchmark::DoNotOptimize(q);
    auto r = q.normalized();
    benchmark::DoNotOptimize(r);
  }
}

template <typename T>
void bm_scalar_mul(benchmark::State& state) {
  Quaternion<T> q(T(0.5), T(0.5), T(0.5), T(0.5));
  T s(2);
  for (auto _ : state) {
    benchmark::DoNotOptimize(q);
    benchmark::DoNotOptimize(s);
    auto r = q * s;
    benchmark::DoNotOptimize(r);
  }
}

}  // namespace

BENCHMARK(bm_multiply<float>);
BENCHMARK(bm_multiply<double>);
BENCHMARK(bm_length<float>);
BENCHMARK(bm_length<double>);
BENCHMARK(bm_normalize<float>);
BENCHMARK(bm_normalize<double>);
BENCHMARK(bm_scalar_mul<float>);
BENCHMARK(bm_scalar_mul<double>);
