#include <benchmark/benchmark.h>

#include "core/Color.h"

#include <vector>

namespace {
  template<typename ColorType>
  void bm_color_add(benchmark::State& state) {
    ColorType a(0.25, 0.5, 0.75);
    ColorType b(0.5, 0.25, 0.125);
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      benchmark::DoNotOptimize(b);
      auto r = a + b;
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename ColorType>
  void bm_color_scalar_mul(benchmark::State& state) {
    ColorType a(0.25, 0.5, 0.75);
    typename ColorType::Component factor(1.5);
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      benchmark::DoNotOptimize(factor);
      auto r = a * factor;
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename ColorType>
  void bm_color_modulate(benchmark::State& state) {
    ColorType a(0.25, 0.5, 0.75);
    ColorType b(0.5, 0.25, 0.125);
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      benchmark::DoNotOptimize(b);
      auto r = a * b;
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename ColorType>
  void bm_color_rgb(benchmark::State& state) {
    ColorType a(0.25, 0.5, 0.75);
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      auto r = a.rgb();
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename ColorType>
  void bm_color_modulate_batch(benchmark::State& state) {
    constexpr int N = 1024;
    std::vector<ColorType> as(N), bs(N);
    for (int i = 0; i != N; ++i) {
      const typename ColorType::Component t =
        static_cast<typename ColorType::Component>(i % 255) / typename ColorType::Component(255);
      as[i] =
        ColorType(t, typename ColorType::Component(1) - t, t * typename ColorType::Component(0.5));
      bs[i] =
        ColorType(typename ColorType::Component(0.25), t, typename ColorType::Component(0.75));
    }

    for (auto _ : state) {
      ColorType acc;
      for (int i = 0; i != N; ++i) {
        acc += as[i] * bs[i];
      }
      benchmark::DoNotOptimize(acc);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * N);
  }
} // namespace

#define REGISTER_FOR(ColorType)                                                                    \
  BENCHMARK(bm_color_add<ColorType>);                                                              \
  BENCHMARK(bm_color_scalar_mul<ColorType>);                                                       \
  BENCHMARK(bm_color_modulate<ColorType>);                                                         \
  BENCHMARK(bm_color_rgb<ColorType>);                                                              \
  BENCHMARK(bm_color_modulate_batch<ColorType>);

REGISTER_FOR(Colorf)
REGISTER_FOR(Colord)

#undef REGISTER_FOR
