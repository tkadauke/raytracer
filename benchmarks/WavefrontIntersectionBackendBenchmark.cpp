#include <benchmark/benchmark.h>

#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "render/GpuIntersectionScene.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/State.h"
#include "render/WavefrontIntersectionBackend.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Torus.h"
#include "render/primitives/Triangle.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace {
  using namespace render;

  struct Workload {
    std::string name;
    std::shared_ptr<Scene> scene;

    [[nodiscard]] WavefrontIntersectionBackendSelectionContext
    selectionContext(std::size_t expectedClosestHitRays, std::size_t expectedAnyHitRays) const {
      return WavefrontIntersectionBackendSelectionContext::fromExpectedQueryFamilies(
        static_cast<std::uint64_t>(expectedClosestHitRays),
        static_cast<std::uint64_t>(expectedAnyHitRays));
    }

    [[nodiscard]] std::shared_ptr<const WavefrontIntersectionBackend>
    automaticBackend(const WavefrontIntersectionBackendSelectionContext& context) const {
      return WavefrontIntersectionBackendChoice::automatic().createBackendForScene(*scene, context);
    }

    void annotateBackendWorkload(benchmark::State& state,
                                 const WavefrontIntersectionBackend& backend,
                                 const WavefrontIntersectionQueryTiming& timing,
                                 const WavefrontIntersectionBackendSelectionContext& context,
                                 std::size_t closestHitRayCount, std::size_t anyHitRayCount,
                                 std::size_t readbackBytes) const {
      const WavefrontIntersectionSceneDiagnostics diagnostics = backend.compiledSceneDiagnostics();
      const WavefrontIntersectionBackendAutoSelectionPolicy policy;
      const std::uint64_t totalRayCount =
        WavefrontIntersectionBackendSelectionContext::saturatedExpectedRayCount(
          static_cast<std::uint64_t>(closestHitRayCount),
          static_cast<std::uint64_t>(anyHitRayCount));
      std::string executionPath = timing.executionPath;
      if (executionPath.empty()) {
        executionPath = backend.executionPath();
      }
      state.SetLabel(name + "/" + backend.requestedName() + "/" + backend.name() + "/" +
                     executionPath);
      state.counters["rays"] = static_cast<double>(totalRayCount);
      state.counters["closest_hit_rays"] = static_cast<double>(closestHitRayCount);
      state.counters["any_hit_rays"] = static_cast<double>(anyHitRayCount);
      state.counters["expected_rays"] = static_cast<double>(context.effectiveExpectedRayCount());
      state.counters["expected_closest_hit_rays"] =
        static_cast<double>(context.expectedClosestHitRayCount);
      state.counters["expected_any_hit_rays"] = static_cast<double>(context.expectedAnyHitRayCount);
      state.counters["auto_minimum_gpu_rays"] =
        static_cast<double>(policy.minimumExpectedRayCount(diagnostics, context));
      state.counters["auto_estimated_query_transfer_bytes"] =
        static_cast<double>(policy.estimatedQueryTransferBytes(diagnostics, context));
      state.counters["scene_compiled"] = diagnostics.compiled ? 1.0 : 0.0;
      state.counters["scene_bvh_nodes"] = static_cast<double>(diagnostics.bvhNodes);
      state.counters["scene_primitives"] = static_cast<double>(diagnostics.primitives);
      state.counters["scene_unsupported"] = static_cast<double>(diagnostics.unsupportedPrimitives);
      state.counters["scene_upload_bytes"] = static_cast<double>(diagnostics.uploadBytes);
      state.counters["packed_closest_hit_eligible"] =
        diagnostics.packedClosestHitKernelEligible ? 1.0 : 0.0;
      state.counters["packed_any_hit_eligible"] =
        diagnostics.packedAnyHitKernelEligible ? 1.0 : 0.0;
      state.counters["ray_upload_bytes"] =
        static_cast<double>(totalRayCount * sizeof(GpuIntersectionRay));
      state.counters["readback_bytes"] = static_cast<double>(readbackBytes);
    }

#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT) || defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    [[nodiscard]] std::shared_ptr<const WavefrontIntersectionBackend>
    requestedGpuBackend(benchmark::State& state) const {
      std::shared_ptr<const WavefrontIntersectionBackend> backend =
        WavefrontIntersectionBackendChoice::gpu().createBackendForScene(*scene);
      if (std::string(backend->availability()) != "available") {
        state.SkipWithError(backend->fallbackReason());
        return nullptr;
      }
      return backend;
    }
#endif
  };

  struct ClosestQueryBatch {
    std::vector<State> states;
    std::vector<WavefrontClosestHitQuery> queries;

    explicit ClosestQueryBatch(const std::vector<Rayd>& rays) {
      states.resize(rays.size());
      queries.reserve(rays.size());
      for (std::size_t index = 0; index != rays.size(); ++index) {
        queries.push_back(WavefrontClosestHitQuery{rays[index], &states[index]});
      }
    }
  };

  struct AnyQueryBatch {
    std::vector<State> states;
    std::vector<WavefrontAnyHitQuery> queries;

    AnyQueryBatch(const std::vector<Rayd>& rays, double maxDistance) {
      states.resize(rays.size());
      queries.reserve(rays.size());
      for (std::size_t index = 0; index != rays.size(); ++index) {
        queries.push_back(WavefrontAnyHitQuery{rays[index], maxDistance, &states[index]});
      }
    }
  };

  std::shared_ptr<Scene> makeSmallSupportedScene() {
    auto scene = std::make_shared<Scene>();
    scene->add(std::make_shared<Sphere>(Vector3d(0, 0, 5), 1.0));
    return scene;
  }

  std::shared_ptr<Scene> makeMeshHeavySupportedScene() {
    auto scene = std::make_shared<Scene>();

    constexpr int side = 40;
    auto height = [](double x, double z) {
      return std::sin(x * 0.31) * 0.45 + std::cos(z * 0.27) * 0.35;
    };
    for (int x = 0; x != side; ++x) {
      for (int z = 0; z != side; ++z) {
        const double x0 = (x - side / 2) * 0.45;
        const double x1 = (x + 1 - side / 2) * 0.45;
        const double z0 = z * 0.45 + 4.0;
        const double z1 = (z + 1) * 0.45 + 4.0;
        const Vector3d p00{x0, height(x0, z0), z0};
        const Vector3d p10{x1, height(x1, z0), z0};
        const Vector3d p01{x0, height(x0, z1), z1};
        const Vector3d p11{x1, height(x1, z1), z1};
        scene->add(std::make_shared<Triangle>(p00, p10, p11));
        scene->add(std::make_shared<Triangle>(p00, p11, p01));
      }
    }

    return scene;
  }

  std::shared_ptr<Scene> makeUnsupportedMixedScene() {
    auto scene = makeMeshHeavySupportedScene();
    scene->add(std::make_shared<Torus>(0.85, 0.2));
    return scene;
  }

  const std::vector<Workload>& workloads() {
    static const std::vector<Workload> all{
      {"small_supported", makeSmallSupportedScene()},
      {"mesh_heavy_supported", makeMeshHeavySupportedScene()},
      {"unsupported_mixed", makeUnsupportedMixedScene()},
    };
    return all;
  }

  const Workload& workloadFor(const benchmark::State& state) {
    return workloads().at(static_cast<std::size_t>(state.range(0)));
  }

  std::vector<Rayd> generateRays(std::int64_t count) {
    std::mt19937 rng(1234);
    std::uniform_real_distribution<double> xy(-8.0, 8.0);
    std::uniform_real_distribution<double> z(3.5, 22.5);
    std::uniform_real_distribution<double> jitter(-0.25, 0.25);

    std::vector<Rayd> rays;
    rays.reserve(static_cast<std::size_t>(count));
    for (std::int64_t index = 0; index != count; ++index) {
      const Vector3d origin(xy(rng), 2.5 + jitter(rng), -5.0);
      const Vector3d target(xy(rng), jitter(rng), z(rng));
      rays.emplace_back(origin, (target - origin).normalized());
    }
    return rays;
  }

  std::vector<GpuIntersectionRay> packRays(const std::vector<Rayd>& rays, double maxDistance) {
    GpuIntersectionScenePacker packer;
    std::vector<GpuIntersectionRay> packed;
    packed.reserve(rays.size());
    for (std::size_t index = 0; index != rays.size(); ++index) {
      packed.push_back(
        packer.packRay(rays[index], static_cast<std::uint32_t>(index), 0.0, maxDistance));
    }
    return packed;
  }

  void annotateScene(benchmark::State& state, const Workload& workload,
                     const CompiledIntersectionScene& compiled,
                     const GpuIntersectionSceneBuffers& buffers) {
    state.SetLabel(workload.name);
    state.counters["primitives"] = static_cast<double>(compiled.primitives().size());
    state.counters["unsupported"] = static_cast<double>(compiled.unsupportedPrimitives().size());
    state.counters["bvh_nodes"] = static_cast<double>(compiled.bvh().size());
    state.counters["upload_bytes"] = static_cast<double>(buffers.uploadByteCount());
  }

  void annotateQuery(benchmark::State& state, const Workload& workload,
                     const GpuIntersectionSceneBuffers& buffers, std::size_t rayCount,
                     std::size_t readbackRecordSize) {
    state.SetLabel(workload.name);
    state.counters["rays"] = static_cast<double>(rayCount);
    state.counters["scene_upload_bytes"] = static_cast<double>(buffers.uploadByteCount());
    state.counters["ray_upload_bytes"] = static_cast<double>(rayCount * sizeof(GpuIntersectionRay));
    state.counters["readback_bytes"] = static_cast<double>(rayCount * readbackRecordSize);
  }

  void bm_compileAndPackScene(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    for (auto _ : state) {
      const CompiledIntersectionScene compiled =
        IntersectionSceneCompiler().compile(*workload.scene);
      const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
      benchmark::DoNotOptimize(compiled.primitives().size());
      benchmark::DoNotOptimize(buffers.uploadByteCount());
    }

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(*workload.scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    annotateScene(state, workload, compiled, buffers);
  }

  void bm_runtimeCpuClosestHit(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::vector<Rayd> rays = generateRays(state.range(1));
    for (auto _ : state) {
      std::size_t hits = 0;
      for (const Rayd& ray : rays) {
        State traceState;
        HitPointInterval hitPoints;
        if (CpuWavefrontIntersectionBackend::instance().intersectClosest(*workload.scene, ray,
                                                                         hitPoints, traceState)) {
          ++hits;
        }
      }
      benchmark::DoNotOptimize(hits);
    }

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(*workload.scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    annotateQuery(state, workload, buffers, rays.size(), sizeof(GpuIntersectionHitRecord));
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(rays.size()));
  }

  void bm_runtimeCpuAnyHit(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::vector<Rayd> rays = generateRays(state.range(1));
    for (auto _ : state) {
      std::size_t hits = 0;
      for (const Rayd& ray : rays) {
        State traceState;
        if (CpuWavefrontIntersectionBackend::instance().intersectAny(*workload.scene, ray, 40.0,
                                                                     traceState)) {
          ++hits;
        }
      }
      benchmark::DoNotOptimize(hits);
    }

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(*workload.scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    annotateQuery(state, workload, buffers, rays.size(), sizeof(GpuIntersectionOcclusionRecord));
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(rays.size()));
  }

  void bm_packedClosestHit(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(*workload.scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    if (!buffers.packedClosestHitKernelEligible()) {
      state.SkipWithError("workload is not packed closest-hit eligible");
      return;
    }

    const std::vector<Rayd> rays = generateRays(state.range(1));
    const std::vector<GpuIntersectionRay> packedRays =
      packRays(rays, std::numeric_limits<double>::infinity());
    for (auto _ : state) {
      const std::vector<GpuIntersectionHitRecord> hits =
        GpuIntersectionIntersector().intersectClosest(buffers, packedRays);
      benchmark::DoNotOptimize(hits.size());
    }

    annotateQuery(state, workload, buffers, packedRays.size(), sizeof(GpuIntersectionHitRecord));
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(packedRays.size()));
  }

  void bm_packedAnyHit(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(*workload.scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    if (!buffers.packedAnyHitKernelEligible()) {
      state.SkipWithError("workload is not packed any-hit eligible");
      return;
    }

    const std::vector<Rayd> rays = generateRays(state.range(1));
    const std::vector<GpuIntersectionRay> packedRays = packRays(rays, 40.0);
    for (auto _ : state) {
      std::size_t hits = 0;
      for (const GpuIntersectionRay& ray : packedRays) {
        if (GpuIntersectionIntersector().intersectAny(buffers, ray)) {
          ++hits;
        }
      }
      benchmark::DoNotOptimize(hits);
    }

    annotateQuery(state, workload, buffers, packedRays.size(),
                  sizeof(GpuIntersectionOcclusionRecord));
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(packedRays.size()));
  }

  void bm_autoClosestHitBatch(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::vector<Rayd> rays = generateRays(state.range(1));
    const WavefrontIntersectionBackendSelectionContext context =
      workload.selectionContext(rays.size(), 0);
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      workload.automaticBackend(context);
    ClosestQueryBatch batch(rays);
    WavefrontIntersectionQueryTiming timing;
    for (auto _ : state) {
      WavefrontIntersectionQueryTiming queryTiming;
      const std::vector<WavefrontClosestHitResult> hits =
        backend->intersectClosestBatch(*workload.scene, batch.queries, &queryTiming);
      timing.add(queryTiming);
      benchmark::DoNotOptimize(hits.size());
    }

    workload.annotateBackendWorkload(state, *backend, timing, context, batch.queries.size(), 0,
                                     batch.queries.size() * sizeof(GpuIntersectionHitRecord));
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(batch.queries.size()));
  }

  void bm_autoAnyHitBatch(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::vector<Rayd> rays = generateRays(state.range(1));
    const WavefrontIntersectionBackendSelectionContext context =
      workload.selectionContext(0, rays.size());
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      workload.automaticBackend(context);
    AnyQueryBatch batch(rays, 40.0);
    WavefrontIntersectionQueryTiming timing;
    for (auto _ : state) {
      WavefrontIntersectionQueryTiming queryTiming;
      const std::vector<bool> occluded =
        backend->intersectAnyBatch(*workload.scene, batch.queries, &queryTiming);
      timing.add(queryTiming);
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), true));
    }

    workload.annotateBackendWorkload(state, *backend, timing, context, 0, batch.queries.size(),
                                     batch.queries.size() * sizeof(GpuIntersectionOcclusionRecord));
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(batch.queries.size()));
  }

  void bm_autoMixedClosestAndAnyHitBatch(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::vector<Rayd> rays = generateRays(state.range(1));
    const WavefrontIntersectionBackendSelectionContext context =
      workload.selectionContext(rays.size(), rays.size());
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      workload.automaticBackend(context);
    ClosestQueryBatch closestBatch(rays);
    AnyQueryBatch anyBatch(rays, 40.0);
    WavefrontIntersectionQueryTiming timing;
    for (auto _ : state) {
      WavefrontIntersectionQueryTiming closestTiming;
      const std::vector<WavefrontClosestHitResult> hits =
        backend->intersectClosestBatch(*workload.scene, closestBatch.queries, &closestTiming);
      timing.add(closestTiming);

      WavefrontIntersectionQueryTiming anyTiming;
      const std::vector<bool> occluded =
        backend->intersectAnyBatch(*workload.scene, anyBatch.queries, &anyTiming);
      timing.add(anyTiming);

      benchmark::DoNotOptimize(hits.size());
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), true));
    }

    workload.annotateBackendWorkload(
      state, *backend, timing, context, closestBatch.queries.size(), anyBatch.queries.size(),
      closestBatch.queries.size() * sizeof(GpuIntersectionHitRecord) +
        anyBatch.queries.size() * sizeof(GpuIntersectionOcclusionRecord));
    state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(closestBatch.queries.size() + anyBatch.queries.size()));
  }

#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT) || defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
  void bm_requestedGpuClosestHitBatch(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      workload.requestedGpuBackend(state);
    if (!backend) {
      return;
    }

    const std::vector<Rayd> rays = generateRays(state.range(1));
    const WavefrontIntersectionBackendSelectionContext context =
      workload.selectionContext(rays.size(), 0);
    ClosestQueryBatch batch(rays);
    WavefrontIntersectionQueryTiming timing;
    for (auto _ : state) {
      WavefrontIntersectionQueryTiming queryTiming;
      const std::vector<WavefrontClosestHitResult> hits =
        backend->intersectClosestBatch(*workload.scene, batch.queries, &queryTiming);
      timing.add(queryTiming);
      benchmark::DoNotOptimize(hits.size());
    }

    workload.annotateBackendWorkload(state, *backend, timing, context, batch.queries.size(), 0,
                                     batch.queries.size() * sizeof(GpuIntersectionHitRecord));
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(batch.queries.size()));
  }

  void bm_requestedGpuAnyHitBatch(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      workload.requestedGpuBackend(state);
    if (!backend) {
      return;
    }

    const std::vector<Rayd> rays = generateRays(state.range(1));
    const WavefrontIntersectionBackendSelectionContext context =
      workload.selectionContext(0, rays.size());
    AnyQueryBatch batch(rays, 40.0);
    WavefrontIntersectionQueryTiming timing;
    for (auto _ : state) {
      WavefrontIntersectionQueryTiming queryTiming;
      const std::vector<bool> occluded =
        backend->intersectAnyBatch(*workload.scene, batch.queries, &queryTiming);
      timing.add(queryTiming);
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), true));
    }

    workload.annotateBackendWorkload(state, *backend, timing, context, 0, batch.queries.size(),
                                     batch.queries.size() * sizeof(GpuIntersectionOcclusionRecord));
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(batch.queries.size()));
  }

  void bm_requestedGpuMixedClosestAndAnyHitBatch(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      workload.requestedGpuBackend(state);
    if (!backend) {
      return;
    }

    const std::vector<Rayd> rays = generateRays(state.range(1));
    const WavefrontIntersectionBackendSelectionContext context =
      workload.selectionContext(rays.size(), rays.size());
    ClosestQueryBatch closestBatch(rays);
    AnyQueryBatch anyBatch(rays, 40.0);
    WavefrontIntersectionQueryTiming timing;
    for (auto _ : state) {
      WavefrontIntersectionQueryTiming closestTiming;
      const std::vector<WavefrontClosestHitResult> hits =
        backend->intersectClosestBatch(*workload.scene, closestBatch.queries, &closestTiming);
      timing.add(closestTiming);

      WavefrontIntersectionQueryTiming anyTiming;
      const std::vector<bool> occluded =
        backend->intersectAnyBatch(*workload.scene, anyBatch.queries, &anyTiming);
      timing.add(anyTiming);

      benchmark::DoNotOptimize(hits.size());
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), true));
    }

    workload.annotateBackendWorkload(
      state, *backend, timing, context, closestBatch.queries.size(), anyBatch.queries.size(),
      closestBatch.queries.size() * sizeof(GpuIntersectionHitRecord) +
        anyBatch.queries.size() * sizeof(GpuIntersectionOcclusionRecord));
    state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(closestBatch.queries.size() + anyBatch.queries.size()));
  }
#endif

  void allWorkloads(benchmark::internal::Benchmark* benchmark) {
    for (int workload = 0; workload != 3; ++workload) {
      benchmark->Arg(workload);
    }
  }

  void supportedQueryWorkloads(benchmark::internal::Benchmark* benchmark) {
    for (int workload = 0; workload != 2; ++workload) {
      benchmark->Args({workload, 256});
      benchmark->Args({workload, 65536});
    }
  }
}

BENCHMARK(bm_compileAndPackScene)->Apply(allWorkloads);
BENCHMARK(bm_runtimeCpuClosestHit)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_runtimeCpuAnyHit)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_packedClosestHit)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_packedAnyHit)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_autoClosestHitBatch)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_autoAnyHitBatch)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_autoMixedClosestAndAnyHitBatch)->Apply(supportedQueryWorkloads);
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT) || defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
BENCHMARK(bm_requestedGpuClosestHitBatch)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_requestedGpuAnyHitBatch)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_requestedGpuMixedClosestAndAnyHitBatch)->Apply(supportedQueryWorkloads);
#endif
