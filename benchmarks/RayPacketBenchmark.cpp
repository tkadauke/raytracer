// Microbenchmarks for the first ray-packet primitive path. The comparison keeps
// the same sphere/ray workload and contrasts Sphere::intersectPacket(Ray4)
// against four scalar Sphere::intersect calls.

#include <benchmark/benchmark.h>

#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "render/State.h"
#include "render/primitives/Sphere.h"

#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace {
  using namespace render;

  std::vector<Rayf> generateRays(int count) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> xy(-0.85f, 0.85f);
    std::uniform_real_distribution<float> z(-3.0f, -2.0f);

    std::vector<Rayf> rays;
    rays.reserve(count);
    for (int i = 0; i != count; ++i) {
      const Vector3f origin(xy(rng), xy(rng), z(rng));
      rays.emplace_back(origin, Vector3f(0, 0, 1));
    }
    return rays;
  }

  std::vector<Ray4> packetize(const std::vector<Rayf>& rays) {
    std::vector<Ray4> packets;
    packets.reserve(rays.size() / Ray4::lanes);
    for (std::size_t i = 0; i < rays.size(); i += Ray4::lanes) {
      packets.emplace_back(std::array<Rayf, 4>{rays[i], rays[i + 1], rays[i + 2], rays[i + 3]});
    }
    return packets;
  }

  Rayd toRayd(const Rayf& ray) {
    return Rayd(
      Vector3d(ray.origin().x(), ray.origin().y(), ray.origin().z()),
      Vector3d(ray.direction().x(), ray.direction().y(), ray.direction().z())
    );
  }

  int countBits(std::uint16_t mask) {
    int count = 0;
    while (mask != 0) {
      count += mask & 1u;
      mask >>= 1u;
    }
    return count;
  }

  void bm_sphere_scalar_four_ray_loop(benchmark::State& state) {
    const Sphere sphere(Vector3d(), 1.0);
    const auto rays = generateRays(4096);

    for (auto _ : state) {
      int hits = 0;
      for (const auto& ray : rays) {
        State traceState;
        HitPointInterval hitPoints;
        if (sphere.intersect(toRayd(ray), hitPoints, traceState)) {
          ++hits;
        }
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * rays.size());
  }

  void bm_sphere_ray4_packet(benchmark::State& state) {
    const Sphere sphere(Vector3d(), 1.0);
    const auto rays = generateRays(4096);
    const auto packets = packetize(rays);

    for (auto _ : state) {
      int hits = 0;
      for (const auto& packet : packets) {
        State traceState;
        const auto result = sphere.intersectPacket(packet, traceState);
        hits += countBits(result.hitMask);
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * rays.size());
  }
}

BENCHMARK(bm_sphere_scalar_four_ray_loop);
BENCHMARK(bm_sphere_ray4_packet);
