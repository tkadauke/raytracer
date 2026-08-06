// Ray4 primitive microbenchmarks for Phase 4.1 packet kernels. Each packet
// benchmark sits next to the equivalent scalar loop and runs at 256-ray and
// 10k-ray batch sizes.

#include <benchmark/benchmark.h>

#include "core/math/BoundingBox.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/simd/Float4.h"
#include "core/util/Bit.h"
#include "render/State.h"
#include "render/primitives/Box.h"
#include "render/primitives/Plane.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Triangle.h"

#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace {
  using namespace render;

  std::vector<Ray4> packetize(const std::vector<Rayf>& rays) {
    std::vector<Ray4> packets;
    packets.reserve(rays.size() / Ray4::lanes);
    for (std::size_t i = 0; i < rays.size(); i += Ray4::lanes) {
      packets.emplace_back(std::array<Rayf, 4>{rays[i], rays[i + 1], rays[i + 2], rays[i + 3]});
    }
    return packets;
  }

  std::vector<Rayf> generateSphereRays(int count) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> xy(-0.9f, 0.9f);
    std::vector<Rayf> rays;
    rays.reserve(count);
    for (int i = 0; i != count; ++i) {
      rays.emplace_back(Vector3f(xy(rng), xy(rng), -2.0f), Vector3f(0, 0, 1));
    }
    return rays;
  }

  std::vector<Rayf> generateTriangleRays(int count) {
    std::mt19937 rng(43);
    std::uniform_real_distribution<float> xy(-1.2f, 1.2f);
    std::vector<Rayf> rays;
    rays.reserve(count);
    for (int i = 0; i != count; ++i) {
      rays.emplace_back(Vector3f(xy(rng), xy(rng), -1.0f), Vector3f(0, 0, 1));
    }
    return rays;
  }

  std::vector<Rayf> generatePlaneRays(int count) {
    std::mt19937 rng(44);
    std::uniform_real_distribution<float> xz(-2.0f, 2.0f);
    std::vector<Rayf> rays;
    rays.reserve(count);
    for (int i = 0; i != count; ++i) {
      rays.emplace_back(Vector3f(xz(rng), 1.0f, xz(rng)), Vector3f(0, -1, 0));
    }
    return rays;
  }

  std::vector<Rayf> generateBoxRays(int count) {
    std::mt19937 rng(45);
    std::uniform_real_distribution<float> xy(-1.4f, 1.4f);
    std::vector<Rayf> rays;
    rays.reserve(count);
    for (int i = 0; i != count; ++i) {
      rays.emplace_back(Vector3f(xy(rng), xy(rng), -2.0f), Vector3f(0, 0, 1));
    }
    return rays;
  }

  std::vector<Rayf> generateBoundingBoxRays(int count) {
    std::mt19937 rng(46);
    std::uniform_real_distribution<float> origin(-4.0f, 4.0f);
    std::uniform_real_distribution<float> direction(-1.0f, 1.0f);
    std::vector<Rayf> rays;
    rays.reserve(count);
    for (int i = 0; i != count; ++i) {
      Vector3f d(direction(rng), direction(rng), direction(rng));
      if (d.length() < 1e-6f) {
        d = Vector3f(1, 0, 0);
      }
      rays.emplace_back(Vector3f(origin(rng), origin(rng), origin(rng)), d.normalized());
    }
    return rays;
  }

  template<typename Primitive, typename RayGenerator>
  void bm_primitive_scalar(benchmark::State& state, const Primitive& primitive,
                           RayGenerator generateRays) {
    const auto rays = generateRays(static_cast<int>(state.range(0)));
    for (auto _ : state) {
      int hits = 0;
      for (const auto& ray : rays) {
        State traceState;
        HitPointInterval hitPoints;
        if (primitive.intersect(Rayd(ray), hitPoints, traceState)) {
          ++hits;
        }
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * rays.size());
  }

  template<typename Primitive, typename RayGenerator>
  void bm_primitive_packet(benchmark::State& state, const Primitive& primitive,
                           RayGenerator generateRays) {
    const auto rays = generateRays(static_cast<int>(state.range(0)));
    const auto packets = packetize(rays);
    for (auto _ : state) {
      int hits = 0;
      for (const auto& packet : packets) {
        State traceState;
        const auto result = primitive.intersectPacket(packet, traceState);
        hits += core::countSetBits(result.hitMask);
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * rays.size());
  }

  void bm_sphere_scalar(benchmark::State& state) {
    bm_primitive_scalar(state, Sphere(Vector3d(), 1.0), generateSphereRays);
  }

  void bm_sphere_packet(benchmark::State& state) {
    bm_primitive_packet(state, Sphere(Vector3d(), 1.0), generateSphereRays);
  }

  void bm_triangle_scalar(benchmark::State& state) {
    bm_primitive_scalar(state,
                        Triangle(Vector3d(-1, -1, 0), Vector3d(-1, 1, 0), Vector3d(1, -1, 0)),
                        generateTriangleRays);
  }

  void bm_triangle_packet(benchmark::State& state) {
    bm_primitive_packet(state,
                        Triangle(Vector3d(-1, -1, 0), Vector3d(-1, 1, 0), Vector3d(1, -1, 0)),
                        generateTriangleRays);
  }

  void bm_plane_scalar(benchmark::State& state) {
    bm_primitive_scalar(state, Plane(Vector3d(0, 1, 0), 0), generatePlaneRays);
  }

  void bm_plane_packet(benchmark::State& state) {
    bm_primitive_packet(state, Plane(Vector3d(0, 1, 0), 0), generatePlaneRays);
  }

  void bm_box_scalar(benchmark::State& state) {
    bm_primitive_scalar(state, Box(Vector3d(), Vector3d(1, 1, 1)), generateBoxRays);
  }

  void bm_box_packet(benchmark::State& state) {
    bm_primitive_packet(state, Box(Vector3d(), Vector3d(1, 1, 1)), generateBoxRays);
  }

  void bm_bounding_box_scalar(benchmark::State& state) {
    const BoundingBoxd box(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    const auto rays = generateBoundingBoxRays(static_cast<int>(state.range(0)));
    for (auto _ : state) {
      int hits = 0;
      for (const auto& ray : rays) {
        if (box.intersects(Rayd(ray))) {
          ++hits;
        }
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * rays.size());
  }

  void bm_bounding_box_packet(benchmark::State& state) {
    const BoundingBoxd box(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    const auto rays = generateBoundingBoxRays(static_cast<int>(state.range(0)));
    const auto packets = packetize(rays);
    for (auto _ : state) {
      int hits = 0;
      for (const auto& packet : packets) {
        hits += core::countSetBits(
          static_cast<std::uint16_t>(core::simd::movemask(box.intersects4(packet))));
      }
      benchmark::DoNotOptimize(hits);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * rays.size());
  }
}

BENCHMARK(bm_sphere_scalar)->Arg(256)->Arg(10000);
BENCHMARK(bm_sphere_packet)->Arg(256)->Arg(10000);
BENCHMARK(bm_triangle_scalar)->Arg(256)->Arg(10000);
BENCHMARK(bm_triangle_packet)->Arg(256)->Arg(10000);
BENCHMARK(bm_plane_scalar)->Arg(256)->Arg(10000);
BENCHMARK(bm_plane_packet)->Arg(256)->Arg(10000);
BENCHMARK(bm_box_scalar)->Arg(256)->Arg(10000);
BENCHMARK(bm_box_packet)->Arg(256)->Arg(10000);
BENCHMARK(bm_bounding_box_scalar)->Arg(256)->Arg(10000);
BENCHMARK(bm_bounding_box_packet)->Arg(256)->Arg(10000);
