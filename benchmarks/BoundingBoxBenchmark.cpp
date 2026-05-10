// Microbenchmarks for BoundingBox<T>. The intersects(Ray) call is the
// hottest single function in the BVH-accelerated renderer — every internal
// BVH node tests it once per ray. The current implementation is a scalar
// slab method with a branch per axis on the ray-direction sign. These
// benchmarks pin the baseline before any SIMD work.

#include <benchmark/benchmark.h>

#include "core/math/BoundingBox.h"
#include "core/math/Ray.h"
#include "core/math/Vector.h"

#include <random>
#include <vector>

namespace {

template <typename T>
std::vector<Rayd> generateRays(int count, T extent) {
  std::mt19937 rng(42);
  std::uniform_real_distribution<double> origin(-double(extent) * 2, double(extent) * 2);
  std::uniform_real_distribution<double> direction(-1.0, 1.0);

  std::vector<Rayd> rays;
  rays.reserve(count);
  for (int i = 0; i < count; ++i) {
    Vector4d o(origin(rng), origin(rng), origin(rng), 1.0);
    Vector3d d(direction(rng), direction(rng), direction(rng));
    if (d.length() < 1e-6) d = Vector3d(1, 0, 0);
    rays.emplace_back(o, d.normalized());
  }
  return rays;
}

template <typename T>
void bm_intersects(benchmark::State& state) {
  BoundingBox<T> box(Vector3<T>(-1, -1, -1), Vector3<T>(1, 1, 1));
  const auto rays = generateRays<T>(256, T(2));
  for (auto _ : state) {
    int hits = 0;
    for (const auto& ray : rays) {
      if (box.intersects(ray)) ++hits;
    }
    benchmark::DoNotOptimize(hits);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * rays.size());
}

template <typename T>
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
      if (box.contains(p)) ++hits;
    }
    benchmark::DoNotOptimize(hits);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * points.size());
}

template <typename T>
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

template <typename T>
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

template <typename T>
void bm_volume(benchmark::State& state) {
  BoundingBox<T> a(Vector3<T>(-1, -1, -1), Vector3<T>(1, 1, 1));
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    auto r = a.volume();
    benchmark::DoNotOptimize(r);
  }
}

template <typename T>
void bm_include_point(benchmark::State& state) {
  std::vector<Vector3<T>> points;
  std::mt19937 rng(11);
  std::uniform_real_distribution<double> u(-1.0, 1.0);
  for (int i = 0; i < 256; ++i) {
    points.emplace_back(T(u(rng)), T(u(rng)), T(u(rng)));
  }
  for (auto _ : state) {
    BoundingBox<T> box;
    for (const auto& p : points) box.include(p);
    benchmark::DoNotOptimize(box);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * points.size());
}

}  // namespace

BENCHMARK(bm_intersects<float>);
BENCHMARK(bm_intersects<double>);

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
