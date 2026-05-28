#include <benchmark/benchmark.h>

#include "core/formats/ply/PlyFile.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "render/State.h"
#include "render/primitives/AccelerationPolicy.h"
#include "render/primitives/Box.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/primitives/SpatialIndexFactory.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Triangle.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
  using namespace render;

  struct Workload {
    std::string name;
    std::vector<std::shared_ptr<Primitive>> primitives;
    std::vector<std::shared_ptr<Primitive>> owners;
    std::vector<Rayd> primaryRays;
    std::vector<Rayd> shadowRays;
  };

  std::filesystem::path repositoryRoot() {
    std::filesystem::path path(__FILE__);
    while (!path.empty() && path.filename() != "benchmarks") {
      path = path.parent_path();
    }
    return path.empty() ? std::filesystem::current_path() : path.parent_path();
  }

  std::shared_ptr<SpatialIndex> buildIndex(const Workload& workload, SpatialIndexKind kind) {
    auto index = makeSpatialIndex(kind);
    for (const auto& primitive : workload.primitives) {
      index->add(primitive);
    }
    index->setup();
    return index;
  }

  std::vector<Rayd> generateRays(int count, const Vector3d& min, const Vector3d& max,
                                 unsigned int seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> y(min.y(), max.y());
    std::uniform_real_distribution<double> z(min.z(), max.z());
    std::uniform_real_distribution<double> jitter(-0.35, 0.35);

    std::vector<Rayd> rays;
    rays.reserve(count);
    for (int i = 0; i != count; ++i) {
      const double t = count <= 1 ? 0.0 : static_cast<double>(i) / static_cast<double>(count - 1);
      const double yy = min.y() + (max.y() - min.y()) * t;
      const double zz = z(rng);
      Vector3d origin(min.x() - 8.0, yy + jitter(rng), zz + jitter(rng));
      Vector3d target(max.x() + 2.0, y(rng), z(rng));
      rays.emplace_back(origin, (target - origin).normalized());
    }
    return rays;
  }

  Workload makeProceduralWorkload() {
    Workload workload;
    workload.name = "procedural_clustered_spheres";

    const std::vector<Vector3d> centers{{-10, -4, -3}, {2, 3, 0}, {14, -2, 4}, {24, 5, -2}};
    for (std::size_t cluster = 0; cluster != centers.size(); ++cluster) {
      for (int x = 0; x != 5; ++x) {
        for (int y = 0; y != 5; ++y) {
          for (int z = 0; z != 4; ++z) {
            const Vector3d p = centers[cluster] + Vector3d(x * 1.25, y * 1.25, z * 1.25);
            workload.primitives.push_back(std::make_shared<Sphere>(p, 0.45));
          }
        }
      }
    }

    workload.primaryRays = generateRays(512, {-14, -7, -6}, {31, 9, 8}, 11);
    workload.shadowRays = generateRays(512, {-2, -10, -7}, {27, 11, 9}, 12);
    return workload;
  }

  Workload makeMeshHeavyWorkload() {
    Workload workload;
    workload.name = "mesh_heavy_terrain";

    constexpr int side = 36;
    for (int x = 0; x != side; ++x) {
      for (int z = 0; z != side; ++z) {
        auto height = [](double px, double pz) {
          return std::sin(px * 0.35) * 0.8 + std::cos(pz * 0.29) * 0.6;
        };
        const double x0 = x * 0.45;
        const double x1 = (x + 1) * 0.45;
        const double z0 = z * 0.45;
        const double z1 = (z + 1) * 0.45;
        const Vector3d p00{x0, height(x0, z0), z0};
        const Vector3d p10{x1, height(x1, z0), z0};
        const Vector3d p01{x0, height(x0, z1), z1};
        const Vector3d p11{x1, height(x1, z1), z1};
        workload.primitives.push_back(std::make_shared<Triangle>(p00, p10, p11));
        workload.primitives.push_back(std::make_shared<Triangle>(p00, p11, p01));
      }
    }

    workload.primaryRays = generateRays(512, {-4, -4, -2}, {22, 4, 19}, 21);
    workload.shadowRays = generateRays(512, {-3, -5, -3}, {21, 5, 20}, 22);
    return workload;
  }

  Workload makeImportedPlyWorkload() {
    Workload workload;
    workload.name = "imported_ply_shark";

    const auto path = repositoryRoot() / "test" / "fixtures" / "shark.ply";
    std::ifstream stream(path);
    if (!stream) {
      throw std::runtime_error("could not open PLY benchmark fixture: " + path.string());
    }

    Mesh mesh;
    PlyFile ply(stream, mesh);
    auto meshPrimitive =
      std::make_shared<MeshPrimitive>(std::move(mesh), MeshPrimitive::NormalMode::Flat);
    workload.owners.push_back(meshPrimitive);
    for (const auto& leaf : meshPrimitive->leaves()) {
      workload.primitives.push_back(leaf);
    }

    workload.primaryRays = generateRays(512, {-72, -20, -18}, {72, 22, 18}, 31);
    workload.shadowRays = generateRays(512, {-64, -22, -20}, {70, 24, 20}, 32);
    return workload;
  }

  Workload makeImportedAssemblyWorkload() {
    Workload workload;
    workload.name = "imported_assembly_mixed_boxes";

    for (int x = 0; x != 18; ++x) {
      for (int y = 0; y != 5; ++y) {
        for (int z = 0; z != 6; ++z) {
          const Vector3d center{x * 1.15, y * 0.8, z * 1.1};
          const Vector3d edge{0.38 + (x % 3) * 0.06, 0.25 + (y % 2) * 0.05,
                              0.36 + (z % 2) * 0.08};
          workload.primitives.push_back(std::make_shared<Box>(center, edge));
        }
      }
    }

    workload.primaryRays = generateRays(512, {-5, -2, -3}, {26, 6, 10}, 41);
    workload.shadowRays = generateRays(512, {-4, -3, -4}, {25, 7, 11}, 42);
    return workload;
  }

  const std::vector<Workload>& workloads() {
    static const std::vector<Workload> all{makeProceduralWorkload(), makeMeshHeavyWorkload(),
                                           makeImportedPlyWorkload(),
                                           makeImportedAssemblyWorkload()};
    return all;
  }

  const Workload& workloadFor(const benchmark::State& state) {
    return workloads().at(static_cast<std::size_t>(state.range(0)));
  }

  SpatialIndexKind kindFor(const benchmark::State& state) {
    return static_cast<SpatialIndexKind>(state.range(1));
  }

  void annotate(benchmark::State& state, const Workload& workload, SpatialIndexKind kind) {
    state.SetLabel(workload.name + "/" + toString(kind));
    state.counters["primitives"] = static_cast<double>(workload.primitives.size());
  }

  void bm_policyBuild(benchmark::State& state) {
    const auto& workload = workloadFor(state);
    const auto kind = kindFor(state);
    for (auto _ : state) {
      auto index = buildIndex(workload, kind);
      benchmark::DoNotOptimize(index);
    }
    annotate(state, workload, kind);
  }

  void bm_policyIntersect(benchmark::State& state) {
    const auto& workload = workloadFor(state);
    const auto kind = kindFor(state);
    const auto index = buildIndex(workload, kind);
    for (auto _ : state) {
      for (const auto& ray : workload.primaryRays) {
        render::State traceState;
        HitPointInterval hits;
        const Primitive* hit = index->intersect(ray, hits, traceState);
        benchmark::DoNotOptimize(hit);
      }
    }
    state.SetItemsProcessed(state.iterations() * workload.primaryRays.size());
    annotate(state, workload, kind);
  }

  void bm_policyShadowRay(benchmark::State& state) {
    const auto& workload = workloadFor(state);
    const auto kind = kindFor(state);
    const auto index = buildIndex(workload, kind);
    for (auto _ : state) {
      for (const auto& ray : workload.shadowRays) {
        render::State traceState;
        bool hit = index->intersects(ray, traceState);
        benchmark::DoNotOptimize(hit);
      }
    }
    state.SetItemsProcessed(state.iterations() * workload.shadowRays.size());
    annotate(state, workload, kind);
  }

  void bm_policyPrimaryRenderImpact(benchmark::State& state) {
    const auto& workload = workloadFor(state);
    const auto kind = kindFor(state);
    const auto index = buildIndex(workload, kind);

    for (auto _ : state) {
      int hits = 0;
      for (const auto& ray : workload.primaryRays) {
        render::State traceState;
        HitPointInterval hitPoints;
        if (index->intersect(ray, hitPoints, traceState)) {
          ++hits;
        }
      }
      benchmark::DoNotOptimize(hits);
    }
    state.SetItemsProcessed(state.iterations() * workload.primaryRays.size());
    annotate(state, workload, kind);
  }

  void policyArgs(benchmark::internal::Benchmark* benchmark) {
    for (int workload = 0; workload != 4; ++workload) {
      benchmark->Args({workload, static_cast<int>(SpatialIndexKind::Linear)});
      benchmark->Args({workload, static_cast<int>(SpatialIndexKind::Grid)});
      benchmark->Args({workload, static_cast<int>(SpatialIndexKind::BVH)});
    }
  }
}

BENCHMARK(bm_policyBuild)->Apply(policyArgs);
BENCHMARK(bm_policyIntersect)->Apply(policyArgs);
BENCHMARK(bm_policyShadowRay)->Apply(policyArgs);
BENCHMARK(bm_policyPrimaryRenderImpact)->Apply(policyArgs);
