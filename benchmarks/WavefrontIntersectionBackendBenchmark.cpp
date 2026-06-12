#include <benchmark/benchmark.h>

#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "render/GpuIntersectionScene.h"
#include "render/IntersectionSceneCompiler.h"
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
#include "render/MetalWavefrontSmokeKernel.h"
#endif
#include "render/State.h"
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
#include "render/VulkanWavefrontSmokeKernel.h"
#endif
#include "render/WavefrontIntersectionBackend.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Torus.h"
#include "render/primitives/Triangle.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
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

    void annotateBackendCompaction(benchmark::State& state,
                                   const WavefrontIntersectionBackend& backend,
                                   const WavefrontFrontierCompactionResult& result) const {
      state.SetLabel(name + "/" + backend.requestedName() + "/" + backend.name() + "/" +
                     result.executionPath());
      state.counters["frontier_compaction_passes"] = 1.0;
      state.counters["frontier_compaction_input_samples"] =
        static_cast<double>(result.inputPathCount());
      state.counters["frontier_compaction_retained_samples"] =
        static_cast<double>(result.retainedPathCount());
      state.counters["frontier_compaction_removed_samples"] =
        static_cast<double>(result.removedPathCount());
      state.counters["frontier_compaction_moved_samples"] =
        static_cast<double>(result.movedPathCount());
      state.counters["frontier_compaction_removed_fraction"] = result.removedPathFraction();
      state.counters["frontier_compaction_moved_retained_fraction"] =
        result.movedRetainedPathFraction();
      state.counters["frontier_compaction_retained_index_bytes"] =
        static_cast<double>(result.retainedIndexBytes());
      state.counters["gpu_frontier_compaction_supported"] =
        backend.supportsGpuFrontierCompaction() ? 1.0 : 0.0;
      state.counters["prepared_ray_batch_compaction_supported"] =
        backend.supportsPreparedRayBatchCompaction() ? 1.0 : 0.0;
    }

    void annotateBackendWorkload(
      benchmark::State& state, const WavefrontIntersectionBackend& backend,
      const WavefrontIntersectionQueryTiming& timing,
      const WavefrontIntersectionBackendSelectionContext& context, std::size_t closestHitRayCount,
      std::size_t anyHitRayCount, std::uint64_t closestHitPackedRayBytes = 0,
      std::uint64_t anyHitPackedRayBytes = 0, std::uint64_t closestHitHostQueryBytes = 0,
      std::uint64_t anyHitHostQueryBytes = 0, std::uint64_t closestHitStateHandleBytes = 0,
      std::uint64_t anyHitStateHandleBytes = 0) const {
      const WavefrontIntersectionSceneDiagnostics diagnostics = backend.compiledSceneDiagnostics();
      const WavefrontIntersectionBackendAutoSelectionPolicy policy;
      const std::uint64_t totalRayCount =
        WavefrontIntersectionBackendSelectionContext::saturatedExpectedRayCount(
          static_cast<std::uint64_t>(closestHitRayCount),
          static_cast<std::uint64_t>(anyHitRayCount));
      const std::uint64_t closestHitRayUploadBytes =
        backend.estimatedClosestHitRayUploadBytes(static_cast<std::uint64_t>(closestHitRayCount));
      const std::uint64_t anyHitRayUploadBytes =
        backend.estimatedAnyHitRayUploadBytes(static_cast<std::uint64_t>(anyHitRayCount));
      const std::uint64_t closestHitReadbackBytes =
        backend.estimatedClosestHitReadbackBytes(static_cast<std::uint64_t>(closestHitRayCount));
      const std::uint64_t anyHitReadbackBytes =
        backend.estimatedAnyHitReadbackBytes(static_cast<std::uint64_t>(anyHitRayCount));
      const bool closestHitRoundTrip = closestHitRayUploadBytes > 0 || closestHitReadbackBytes > 0;
      const bool anyHitRoundTrip = anyHitRayUploadBytes > 0 || anyHitReadbackBytes > 0;
      const std::uint64_t queryRoundTrips =
        frontierQueryRoundTrips(closestHitRoundTrip, anyHitRoundTrip);
      const std::uint64_t residentQueryRoundTrips =
        residentFrontierQueryRoundTripsEstimate(closestHitRoundTrip, anyHitRoundTrip);
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
      annotateUnsupportedReasons(state, diagnostics.unsupportedReasons);
      state.counters["scene_upload_bytes"] = static_cast<double>(diagnostics.uploadBytes);
      state.counters["packed_closest_hit_eligible"] =
        diagnostics.packedClosestHitKernelEligible ? 1.0 : 0.0;
      state.counters["packed_any_hit_eligible"] =
        diagnostics.packedAnyHitKernelEligible ? 1.0 : 0.0;
      state.counters["ray_upload_bytes"] =
        static_cast<double>(closestHitRayUploadBytes + anyHitRayUploadBytes);
      state.counters["closest_hit_ray_upload_bytes"] =
        static_cast<double>(closestHitRayUploadBytes);
      state.counters["any_hit_ray_upload_bytes"] = static_cast<double>(anyHitRayUploadBytes);
      state.counters["closest_hit_readback_bytes"] = static_cast<double>(closestHitReadbackBytes);
      state.counters["any_hit_readback_bytes"] = static_cast<double>(anyHitReadbackBytes);
      state.counters["readback_bytes"] =
        static_cast<double>(closestHitReadbackBytes + anyHitReadbackBytes);
      state.counters["closest_hit_query_transfer_bytes"] =
        static_cast<double>(closestHitRayUploadBytes + closestHitReadbackBytes);
      state.counters["any_hit_query_transfer_bytes"] =
        static_cast<double>(anyHitRayUploadBytes + anyHitReadbackBytes);
      state.counters["frontier_packed_ray_bytes"] =
        static_cast<double>(closestHitPackedRayBytes + anyHitPackedRayBytes);
      state.counters["closest_hit_frontier_packed_ray_bytes"] =
        static_cast<double>(closestHitPackedRayBytes);
      state.counters["any_hit_frontier_packed_ray_bytes"] =
        static_cast<double>(anyHitPackedRayBytes);
      state.counters["frontier_host_query_bytes"] =
        static_cast<double>(closestHitHostQueryBytes + anyHitHostQueryBytes);
      state.counters["closest_hit_frontier_host_query_bytes"] =
        static_cast<double>(closestHitHostQueryBytes);
      state.counters["any_hit_frontier_host_query_bytes"] =
        static_cast<double>(anyHitHostQueryBytes);
      state.counters["frontier_state_handle_bytes"] =
        static_cast<double>(closestHitStateHandleBytes + anyHitStateHandleBytes);
      state.counters["closest_hit_frontier_state_handle_bytes"] =
        static_cast<double>(closestHitStateHandleBytes);
      state.counters["any_hit_frontier_state_handle_bytes"] =
        static_cast<double>(anyHitStateHandleBytes);
      state.counters["query_round_trips"] = static_cast<double>(queryRoundTrips);
      state.counters["closest_hit_query_round_trips"] = closestHitRoundTrip ? 1.0 : 0.0;
      state.counters["any_hit_query_round_trips"] = anyHitRoundTrip ? 1.0 : 0.0;
      state.counters["frontier_resident_query_round_trips_estimate"] =
        static_cast<double>(residentQueryRoundTrips);
      state.counters["frontier_resident_query_round_trip_savings_estimate"] =
        static_cast<double>(queryRoundTrips - residentQueryRoundTrips);
      state.counters["resident_frontiers_supported"] =
        backend.supportsResidentFrontiers() ? 1.0 : 0.0;
      state.counters["gpu_frontier_compaction_supported"] =
        backend.supportsGpuFrontierCompaction() ? 1.0 : 0.0;
      state.counters["prepared_ray_batch_compaction_supported"] =
        backend.supportsPreparedRayBatchCompaction() ? 1.0 : 0.0;
      state.counters["resident_direct_light_batches_supported"] =
        backend.supportsResidentDirectLightBatches() ? 1.0 : 0.0;
    }

    [[nodiscard]] std::shared_ptr<const WavefrontIntersectionBackend>
    requestedGpuBackend(benchmark::State& state) const {
      std::shared_ptr<const WavefrontIntersectionBackend> backend =
        WavefrontIntersectionBackendChoice::gpu().createBackendForScene(*scene);
      if (!backend) {
        state.SkipWithError("GPU backend creation failed");
      }
      return backend;
    }

#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT) || defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    [[nodiscard]] std::shared_ptr<const WavefrontIntersectionBackend>
    requestedAvailableGpuBackend(benchmark::State& state) const {
      std::shared_ptr<const WavefrontIntersectionBackend> backend = requestedGpuBackend(state);
      if (!backend) {
        return nullptr;
      }
      if (std::string(backend->availability()) != "available") {
        state.SkipWithError(backend->fallbackReason());
        return nullptr;
      }
      return backend;
    }
#endif

    void annotateCompiledScene(benchmark::State& state, const CompiledIntersectionScene& compiled,
                               const GpuIntersectionSceneBuffers& buffers) const {
      state.SetLabel(name);
      state.counters["primitives"] = static_cast<double>(compiled.primitives().size());
      state.counters["unsupported"] = static_cast<double>(compiled.unsupportedPrimitives().size());
      annotateUnsupportedReasons(state, compiled.unsupportedReasonCounts());
      state.counters["bvh_nodes"] = static_cast<double>(compiled.bvh().size());
      state.counters["upload_bytes"] = static_cast<double>(buffers.uploadByteCount());
    }

    void annotateUnsupportedReasons(benchmark::State& state,
                                    const std::map<std::string, std::uint64_t>& reasons) const {
      for (const auto& [reason, count] : reasons) {
        state.counters[unsupportedReasonCounterName(reason)] = static_cast<double>(count);
      }
    }

    void annotateUnsupportedReasons(
      benchmark::State& state,
      const std::vector<UnsupportedIntersectionReasonCount>& reasons) const {
      for (const UnsupportedIntersectionReasonCount& reason : reasons) {
        state.counters[unsupportedReasonCounterName(reason.reason)] =
          static_cast<double>(reason.count);
      }
    }

    static std::string unsupportedReasonCounterName(std::string reason) {
      if (reason.empty()) {
        reason = "unknown";
      }
      for (char& ch : reason) {
        if (!std::isalnum(static_cast<unsigned char>(ch))) {
          ch = '_';
        }
      }
      return "scene_unsupported_reason_" + reason;
    }

    [[nodiscard]] std::uint64_t frontierQueryRoundTrips(bool closestHitRoundTrip,
                                                        bool anyHitRoundTrip) const {
      return (closestHitRoundTrip ? 1u : 0u) + (anyHitRoundTrip ? 1u : 0u);
    }

    [[nodiscard]] std::uint64_t
    residentFrontierQueryRoundTripsEstimate(bool closestHitRoundTrip, bool anyHitRoundTrip) const {
      const std::uint64_t current = frontierQueryRoundTrips(closestHitRoundTrip, anyHitRoundTrip);
      if (closestHitRoundTrip && anyHitRoundTrip) {
        return 1;
      }
      return current;
    }
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

  WavefrontFrontierCompactionRequest makeCompactionRequest(std::int64_t inputPathCount) {
    WavefrontFrontierCompactionRequest request(static_cast<std::size_t>(inputPathCount));
    for (std::int64_t index = 1; index < inputPathCount; index += 2) {
      request.retain(static_cast<std::size_t>(index));
    }
    return request;
  }

#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT) || defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
  std::vector<std::uint32_t> retainedRayIndices(std::int64_t inputRayCount) {
    std::vector<std::uint32_t> indices;
    indices.reserve(static_cast<std::size_t>(inputRayCount / 2));
    for (std::int64_t index = 1; index < inputRayCount; index += 2) {
      indices.push_back(static_cast<std::uint32_t>(index));
    }
    return indices;
  }
#endif

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
    workload.annotateCompiledScene(state, compiled, buffers);
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
    const std::unique_ptr<WavefrontClosestHitFrontier> frontier =
      backend->createClosestHitFrontier(batch.queries);
    WavefrontIntersectionQueryTiming timing;
    for (auto _ : state) {
      WavefrontIntersectionQueryTiming queryTiming;
      const std::vector<WavefrontClosestHitResult> hits =
        backend->intersectClosestFrontier(*workload.scene, *frontier, &queryTiming);
      timing.add(queryTiming);
      benchmark::DoNotOptimize(hits.size());
    }

    workload.annotateBackendWorkload(state, *backend, timing, context, frontier->rayCount(), 0,
                                     frontier->packedRayBytes(), 0, frontier->hostQueryBytes(), 0,
                                     frontier->stateHandleBytes(), 0);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(frontier->rayCount()));
  }

  void bm_autoAnyHitBatch(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::vector<Rayd> rays = generateRays(state.range(1));
    const WavefrontIntersectionBackendSelectionContext context =
      workload.selectionContext(0, rays.size());
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      workload.automaticBackend(context);
    AnyQueryBatch batch(rays, 40.0);
    const std::unique_ptr<WavefrontAnyHitFrontier> frontier =
      backend->createAnyHitFrontier(batch.queries);
    WavefrontIntersectionQueryTiming timing;
    for (auto _ : state) {
      WavefrontIntersectionQueryTiming queryTiming;
      const std::vector<bool> occluded =
        backend->intersectAnyFrontier(*workload.scene, *frontier, &queryTiming);
      timing.add(queryTiming);
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), true));
    }

    workload.annotateBackendWorkload(state, *backend, timing, context, 0, frontier->rayCount(), 0,
                                     frontier->packedRayBytes(), 0, frontier->hostQueryBytes(), 0,
                                     frontier->stateHandleBytes());
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(frontier->rayCount()));
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
    const std::unique_ptr<WavefrontClosestHitFrontier> closestFrontier =
      backend->createClosestHitFrontier(closestBatch.queries);
    const std::unique_ptr<WavefrontAnyHitFrontier> anyFrontier =
      backend->createAnyHitFrontier(anyBatch.queries);
    WavefrontIntersectionQueryTiming timing;
    for (auto _ : state) {
      WavefrontIntersectionQueryTiming closestTiming;
      const std::vector<WavefrontClosestHitResult> hits =
        backend->intersectClosestFrontier(*workload.scene, *closestFrontier, &closestTiming);
      timing.add(closestTiming);

      WavefrontIntersectionQueryTiming anyTiming;
      const std::vector<bool> occluded =
        backend->intersectAnyFrontier(*workload.scene, *anyFrontier, &anyTiming);
      timing.add(anyTiming);

      benchmark::DoNotOptimize(hits.size());
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), true));
    }

    workload.annotateBackendWorkload(
      state, *backend, timing, context, closestFrontier->rayCount(), anyFrontier->rayCount(),
      closestFrontier->packedRayBytes(), anyFrontier->packedRayBytes(),
      closestFrontier->hostQueryBytes(), anyFrontier->hostQueryBytes(),
      closestFrontier->stateHandleBytes(), anyFrontier->stateHandleBytes());
    state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(closestFrontier->rayCount() + anyFrontier->rayCount()));
  }

  void bm_autoFrontierCompaction(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const WavefrontIntersectionBackendSelectionContext context = workload.selectionContext(
      static_cast<std::size_t>(state.range(1)), static_cast<std::size_t>(state.range(1)));
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      workload.automaticBackend(context);
    const WavefrontFrontierCompactionRequest request = makeCompactionRequest(state.range(1));
    WavefrontFrontierCompactionResult result = backend->compactFrontier(request);
    for (auto _ : state) {
      result = backend->compactFrontier(request);
      benchmark::DoNotOptimize(result.retainedPathCount());
    }

    workload.annotateBackendCompaction(state, *backend, result);
    state.SetItemsProcessed(state.iterations() * state.range(1));
  }

  void bm_requestedGpuUnsupportedMixedClosestAndAnyHitBatch(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      workload.requestedGpuBackend(state);
    if (!backend) {
      return;
    }

    const WavefrontIntersectionSceneDiagnostics diagnostics = backend->compiledSceneDiagnostics();
    if (std::string(backend->availability()) != "fallback" ||
        diagnostics.unsupportedPrimitives == 0) {
      state.SkipWithError("workload did not produce an unsupported GPU fallback");
      return;
    }

    const std::vector<Rayd> rays = generateRays(state.range(1));
    const WavefrontIntersectionBackendSelectionContext context =
      workload.selectionContext(rays.size(), rays.size());
    ClosestQueryBatch closestBatch(rays);
    AnyQueryBatch anyBatch(rays, 40.0);
    const std::unique_ptr<WavefrontClosestHitFrontier> closestFrontier =
      backend->createClosestHitFrontier(closestBatch.queries);
    const std::unique_ptr<WavefrontAnyHitFrontier> anyFrontier =
      backend->createAnyHitFrontier(anyBatch.queries);
    WavefrontIntersectionQueryTiming timing;
    for (auto _ : state) {
      WavefrontIntersectionQueryTiming closestTiming;
      const std::vector<WavefrontClosestHitResult> hits =
        backend->intersectClosestFrontier(*workload.scene, *closestFrontier, &closestTiming);
      timing.add(closestTiming);

      WavefrontIntersectionQueryTiming anyTiming;
      const std::vector<bool> occluded =
        backend->intersectAnyFrontier(*workload.scene, *anyFrontier, &anyTiming);
      timing.add(anyTiming);

      benchmark::DoNotOptimize(hits.size());
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), true));
    }

    workload.annotateBackendWorkload(
      state, *backend, timing, context, closestFrontier->rayCount(), anyFrontier->rayCount(),
      closestFrontier->packedRayBytes(), anyFrontier->packedRayBytes(),
      closestFrontier->hostQueryBytes(), anyFrontier->hostQueryBytes(),
      closestFrontier->stateHandleBytes(), anyFrontier->stateHandleBytes());
    state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(closestFrontier->rayCount() + anyFrontier->rayCount()));
  }

#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT) || defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
  void bm_requestedGpuClosestHitBatch(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      workload.requestedAvailableGpuBackend(state);
    if (!backend) {
      return;
    }

    const std::vector<Rayd> rays = generateRays(state.range(1));
    const WavefrontIntersectionBackendSelectionContext context =
      workload.selectionContext(rays.size(), 0);
    ClosestQueryBatch batch(rays);
    const std::unique_ptr<WavefrontClosestHitFrontier> frontier =
      backend->createClosestHitFrontier(batch.queries);
    WavefrontIntersectionQueryTiming timing;
    for (auto _ : state) {
      WavefrontIntersectionQueryTiming queryTiming;
      const std::vector<WavefrontClosestHitResult> hits =
        backend->intersectClosestFrontier(*workload.scene, *frontier, &queryTiming);
      timing.add(queryTiming);
      benchmark::DoNotOptimize(hits.size());
    }

    workload.annotateBackendWorkload(state, *backend, timing, context, frontier->rayCount(), 0,
                                     frontier->packedRayBytes(), 0, frontier->hostQueryBytes(), 0,
                                     frontier->stateHandleBytes(), 0);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(frontier->rayCount()));
  }

  void bm_requestedGpuAnyHitBatch(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      workload.requestedAvailableGpuBackend(state);
    if (!backend) {
      return;
    }

    const std::vector<Rayd> rays = generateRays(state.range(1));
    const WavefrontIntersectionBackendSelectionContext context =
      workload.selectionContext(0, rays.size());
    AnyQueryBatch batch(rays, 40.0);
    const std::unique_ptr<WavefrontAnyHitFrontier> frontier =
      backend->createAnyHitFrontier(batch.queries);
    WavefrontIntersectionQueryTiming timing;
    for (auto _ : state) {
      WavefrontIntersectionQueryTiming queryTiming;
      const std::vector<bool> occluded =
        backend->intersectAnyFrontier(*workload.scene, *frontier, &queryTiming);
      timing.add(queryTiming);
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), true));
    }

    workload.annotateBackendWorkload(state, *backend, timing, context, 0, frontier->rayCount(), 0,
                                     frontier->packedRayBytes(), 0, frontier->hostQueryBytes(), 0,
                                     frontier->stateHandleBytes());
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(frontier->rayCount()));
  }

  void bm_requestedGpuMixedClosestAndAnyHitBatch(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      workload.requestedAvailableGpuBackend(state);
    if (!backend) {
      return;
    }

    const std::vector<Rayd> rays = generateRays(state.range(1));
    const WavefrontIntersectionBackendSelectionContext context =
      workload.selectionContext(rays.size(), rays.size());
    ClosestQueryBatch closestBatch(rays);
    AnyQueryBatch anyBatch(rays, 40.0);
    const std::unique_ptr<WavefrontClosestHitFrontier> closestFrontier =
      backend->createClosestHitFrontier(closestBatch.queries);
    const std::unique_ptr<WavefrontAnyHitFrontier> anyFrontier =
      backend->createAnyHitFrontier(anyBatch.queries);
    WavefrontIntersectionQueryTiming timing;
    for (auto _ : state) {
      WavefrontIntersectionQueryTiming closestTiming;
      const std::vector<WavefrontClosestHitResult> hits =
        backend->intersectClosestFrontier(*workload.scene, *closestFrontier, &closestTiming);
      timing.add(closestTiming);

      WavefrontIntersectionQueryTiming anyTiming;
      const std::vector<bool> occluded =
        backend->intersectAnyFrontier(*workload.scene, *anyFrontier, &anyTiming);
      timing.add(anyTiming);

      benchmark::DoNotOptimize(hits.size());
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), true));
    }

    workload.annotateBackendWorkload(
      state, *backend, timing, context, closestFrontier->rayCount(), anyFrontier->rayCount(),
      closestFrontier->packedRayBytes(), anyFrontier->packedRayBytes(),
      closestFrontier->hostQueryBytes(), anyFrontier->hostQueryBytes(),
      closestFrontier->stateHandleBytes(), anyFrontier->stateHandleBytes());
    state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(closestFrontier->rayCount() + anyFrontier->rayCount()));
  }

  void bm_requestedGpuFrontierCompaction(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      workload.requestedAvailableGpuBackend(state);
    if (!backend) {
      return;
    }

    const WavefrontFrontierCompactionRequest request = makeCompactionRequest(state.range(1));
    WavefrontFrontierCompactionResult result = backend->compactFrontier(request);
    for (auto _ : state) {
      result = backend->compactFrontier(request);
      benchmark::DoNotOptimize(result.retainedPathCount());
    }

    workload.annotateBackendCompaction(state, *backend, result);
    state.SetItemsProcessed(state.iterations() * state.range(1));
  }

  void annotatePreparedRayBatchCompaction(benchmark::State& state, const Workload& workload,
                                          const char* platformName, std::size_t inputRayCount,
                                          std::size_t retainedRayCount) {
    state.SetLabel(workload.name + "/" + platformName + "/prepared_ray_batch_compaction");
    state.counters["frontier_compaction_input_samples"] = static_cast<double>(inputRayCount);
    state.counters["frontier_compaction_retained_samples"] = static_cast<double>(retainedRayCount);
    state.counters["frontier_compaction_removed_samples"] =
      static_cast<double>(inputRayCount - retainedRayCount);
    state.counters["frontier_compaction_removed_fraction"] =
      inputRayCount == 0
        ? 0.0
        : 1.0 - static_cast<double>(retainedRayCount) / static_cast<double>(inputRayCount);
    state.counters["frontier_packed_ray_bytes"] =
      static_cast<double>(inputRayCount * sizeof(GpuIntersectionRay));
    state.counters["compacted_frontier_packed_ray_bytes"] =
      static_cast<double>(retainedRayCount * sizeof(GpuIntersectionRay));
    state.counters["frontier_compaction_retained_index_bytes"] =
      static_cast<double>(retainedRayCount * sizeof(std::uint32_t));
    state.counters["prepared_ray_batch_compaction_supported"] = 1.0;
    state.counters["gpu_frontier_compaction_supported"] = 0.0;
  }

#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
  void bm_metalPreparedRayBatchCompaction(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const MetalWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      state.SkipWithError(kernel.deviceUnavailableReason());
      return;
    }
    if (!kernel.renderPathAvailable()) {
      state.SkipWithError(kernel.renderPathUnavailableReason());
      return;
    }

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(*workload.scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    if (!buffers.basicHitKernelEligible()) {
      state.SkipWithError("workload is not Metal basic-hit eligible");
      return;
    }

    const std::vector<Rayd> rays = generateRays(state.range(1));
    const std::vector<GpuIntersectionRay> packedRays =
      packRays(rays, std::numeric_limits<double>::infinity());
    const std::vector<std::uint32_t> retainedIndices = retainedRayIndices(state.range(1));
    const MetalWavefrontPreparedScene prepared(buffers);
    const std::shared_ptr<const MetalWavefrontPreparedRayBatch> sourceBatch =
      prepared.prepareRays(packedRays);

    std::shared_ptr<const MetalWavefrontPreparedRayBatch> compacted =
      prepared.compactRays(*sourceBatch, retainedIndices);
    for (auto _ : state) {
      compacted = prepared.compactRays(*sourceBatch, retainedIndices);
      benchmark::DoNotOptimize(compacted->rayCount());
    }

    annotatePreparedRayBatchCompaction(state, workload, "metal", packedRays.size(),
                                       retainedIndices.size());
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(packedRays.size()));
  }
#endif

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
  void bm_vulkanPreparedRayBatchCompaction(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const VulkanWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      state.SkipWithError(kernel.deviceUnavailableReason());
      return;
    }
    if (!kernel.renderPathAvailable()) {
      state.SkipWithError(kernel.renderPathUnavailableReason());
      return;
    }

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(*workload.scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    if (!buffers.basicHitKernelEligible()) {
      state.SkipWithError("workload is not Vulkan basic-hit eligible");
      return;
    }

    const std::vector<Rayd> rays = generateRays(state.range(1));
    const std::vector<GpuIntersectionRay> packedRays =
      packRays(rays, std::numeric_limits<double>::infinity());
    const std::vector<std::uint32_t> retainedIndices = retainedRayIndices(state.range(1));
    const VulkanWavefrontPreparedScene prepared(buffers);
    const std::shared_ptr<const VulkanWavefrontPreparedRayBatch> sourceBatch =
      prepared.prepareRays(packedRays);

    std::shared_ptr<const VulkanWavefrontPreparedRayBatch> compacted =
      prepared.compactRays(*sourceBatch, retainedIndices);
    for (auto _ : state) {
      compacted = prepared.compactRays(*sourceBatch, retainedIndices);
      benchmark::DoNotOptimize(compacted->rayCount());
    }

    annotatePreparedRayBatchCompaction(state, workload, "vulkan", packedRays.size(),
                                       retainedIndices.size());
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(packedRays.size()));
  }
#endif
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

  void unsupportedQueryWorkloads(benchmark::internal::Benchmark* benchmark) {
    benchmark->Args({2, 256});
    benchmark->Args({2, 65536});
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
BENCHMARK(bm_autoFrontierCompaction)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_requestedGpuUnsupportedMixedClosestAndAnyHitBatch)->Apply(unsupportedQueryWorkloads);
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT) || defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
BENCHMARK(bm_requestedGpuClosestHitBatch)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_requestedGpuAnyHitBatch)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_requestedGpuMixedClosestAndAnyHitBatch)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_requestedGpuFrontierCompaction)->Apply(supportedQueryWorkloads);
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
BENCHMARK(bm_metalPreparedRayBatchCompaction)->Apply(supportedQueryWorkloads);
#endif
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
BENCHMARK(bm_vulkanPreparedRayBatchCompaction)->Apply(supportedQueryWorkloads);
#endif
#endif
