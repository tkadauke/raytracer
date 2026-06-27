#include <benchmark/benchmark.h>

#include "core/Buffer.h"
#include "core/geometry/Polyline.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "render/GpuDiffusePathLoopBackend.h"
#include "render/GpuDiffusePathStepReference.h"
#include "render/GpuIntersectionScene.h"
#include "render/GpuTracingScene.h"
#include "render/IntersectionService.h"
#include "render/IntersectionSceneCompiler.h"
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
#include "render/MetalWavefrontSmokeKernel.h"
#endif
#include "render/State.h"
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
#include "render/VulkanWavefrontSmokeKernel.h"
#endif
#include "render/WavefrontFrontierCompaction.h"
#include "render/WavefrontIntersectionBackend.h"
#include "render/lights/PointLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Curve.h"
#include "render/primitives/Rectangle.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Triangle.h"
#include "render/textures/ConstantColorTexture.h"

#include <algorithm>
#include <cctype>
#include <chrono>
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
      state.counters["frontier_compaction_upload_seconds"] = result.timing().uploadSeconds;
      state.counters["frontier_compaction_kernel_seconds"] = result.timing().kernelSeconds;
      state.counters["frontier_compaction_readback_seconds"] = result.timing().readbackSeconds;
      annotateBackendCapabilityCounters(state, backend);
    }

    void annotateCompiledDiffusePathLoop(benchmark::State& state,
                                         const GpuTracingSceneCompilation& compilation,
                                         const GpuDiffusePathLoopResult& result,
                                         double measuredSeconds) const {
      const GpuDiffusePathStepMetrics& metrics = result.metrics;
      state.SetLabel(name + "/compiled_diffuse_path_loop/" + result.executionPath + "/" +
                     result.pathStateResidency + "/" + result.platformLabel());

      state.counters["compiled_path_loop_initial_paths"] =
        static_cast<double>(result.initialPathCount);
      state.counters["compiled_path_loop_depths"] = static_cast<double>(result.depthCount);
      state.counters["compiled_path_loop_active_paths"] =
        static_cast<double>(result.inputPathCount());
      state.counters["resident_path_loop_peak_active_paths"] =
        static_cast<double>(result.peakActivePathCount());
      state.counters["resident_path_loop_last_active_paths"] =
        static_cast<double>(result.lastActivePathCount());
      state.counters["compiled_path_loop_closest_hit_rays"] =
        static_cast<double>(metrics.closestHitRays);
      state.counters["compiled_path_loop_direct_light_visibility_rays"] =
        static_cast<double>(metrics.directLightVisibilityRays);
      state.counters["compiled_path_loop_direct_light_samples"] =
        static_cast<double>(metrics.directLightSamples);
      state.counters["compiled_path_loop_direct_light_contribution_evaluations"] =
        static_cast<double>(metrics.directLightContributionEvaluations);
      state.counters["compiled_path_loop_hits"] = static_cast<double>(metrics.hits);
      state.counters["compiled_path_loop_misses"] = static_cast<double>(metrics.misses);
      state.counters["compiled_path_loop_unsupported_hits"] =
        static_cast<double>(metrics.unsupportedHits);
      state.counters["compiled_path_loop_spawned_continuations"] =
        static_cast<double>(result.retainedPathCount());
      state.counters["compiled_path_loop_terminated_paths"] =
        static_cast<double>(metrics.terminatedPaths);
      state.counters["compiled_path_loop_max_depth_terminated_paths"] =
        static_cast<double>(result.maxDepthTerminatedPaths);
      state.counters["resident_path_loop_compaction_passes"] =
        static_cast<double>(result.compactionPassCount());
      state.counters["resident_path_loop_input_paths"] =
        static_cast<double>(result.inputPathCount());
      state.counters["resident_path_loop_retained_paths"] =
        static_cast<double>(result.retainedPathCount());
      state.counters["resident_path_loop_removed_paths"] =
        static_cast<double>(result.removedPathCount());
      state.counters["resident_path_loop_moved_paths"] =
        static_cast<double>(result.movedPathCount());
      state.counters["resident_path_loop_removed_fraction"] = result.removedPathFraction();
      state.counters["resident_path_loop_moved_retained_fraction"] =
        result.movedRetainedPathFraction();
      state.counters["resident_path_loop_retained_index_bytes"] =
        static_cast<double>(result.retainedPathIndexBytes());
      state.counters["resident_path_loop_resident_path_state_bytes"] =
        static_cast<double>(result.residentPathStateBytes());
      state.counters["resident_path_loop_input_resident_path_state_bytes"] =
        static_cast<double>(result.inputPathStateBytes());
      state.counters["resident_path_loop_retained_resident_path_state_bytes"] =
        static_cast<double>(result.retainedPathStateBytes());
      state.counters["resident_path_loop_removed_resident_path_state_bytes"] =
        static_cast<double>(result.removedPathStateBytes());
      state.counters["resident_path_loop_round_trips"] = static_cast<double>(result.roundTrips);
      state.counters["resident_path_loop_saved_host_readbacks"] =
        static_cast<double>(result.savedHostReadbacks);
      state.counters["resident_path_loop_saved_host_readback_bytes"] =
        static_cast<double>(result.savedHostReadbackBytes);
      state.counters["resident_path_loop_submitted_intersection_rays"] =
        static_cast<double>(result.submittedIntersectionRayCount());
      state.counters["full_gpu_path_loop_supported"] =
        result.fullGpuPathLoopSupported() ? 1.0 : 0.0;
      state.counters["full_gpu_path_loop_unavailable"] =
        result.fullGpuPathLoopUnavailable() ? 1.0 : 0.0;
      state.counters["full_gpu_path_loop_upload_seconds"] =
        result.frontierCompactionUploadWorkerSeconds;
      state.counters["full_gpu_path_loop_kernel_seconds"] =
        result.frontierCompactionKernelWorkerSeconds;
      state.counters["full_gpu_path_loop_readback_seconds"] =
        result.frontierCompactionReadbackWorkerSeconds;
      const double fullGpuPathLoopReportedSeconds = result.frontierCompactionUploadWorkerSeconds +
                                                    result.frontierCompactionKernelWorkerSeconds +
                                                    result.frontierCompactionReadbackWorkerSeconds;
      state.counters["full_gpu_path_loop_reported_seconds"] = fullGpuPathLoopReportedSeconds;
      state.counters["full_gpu_path_loop_host_overhead_seconds"] =
        std::max(0.0, measuredSeconds - fullGpuPathLoopReportedSeconds);

      const GpuTracingSceneDiagnostics& diagnostics = compilation.diagnostics;
      state.counters["tracing_scene_materials"] = static_cast<double>(diagnostics.materials);
      state.counters["tracing_scene_textures"] = static_cast<double>(diagnostics.textures);
      state.counters["tracing_scene_lights"] = static_cast<double>(diagnostics.lights);
      state.counters["tracing_scene_environment"] = static_cast<double>(diagnostics.environment);
      state.counters["tracing_scene_upload_bytes"] = static_cast<double>(diagnostics.uploadBytes);
      state.counters["tracing_scene_unsupported_primitives"] =
        static_cast<double>(diagnostics.unsupportedPrimitives);
      state.counters["tracing_scene_unsupported_materials"] =
        static_cast<double>(diagnostics.unsupportedMaterials);
      state.counters["tracing_scene_unsupported_textures"] =
        static_cast<double>(diagnostics.unsupportedTextures);
      state.counters["tracing_scene_unsupported_lights"] =
        static_cast<double>(diagnostics.unsupportedLights);

      WavefrontIntersectionQueryTiming timing;
      if (result.fullGpuPathLoopSupported()) {
        timing.uploadSeconds = result.frontierCompactionUploadWorkerSeconds;
        timing.kernelSeconds = result.frontierCompactionKernelWorkerSeconds;
        timing.readbackSeconds = result.frontierCompactionReadbackWorkerSeconds;
      } else {
        timing.kernelSeconds = measuredSeconds;
      }
      timing.executionPath = result.executionPath;
      annotateComparableTracingMetrics(state, result.submittedIntersectionRayCount(),
                                       measuredSeconds, timing, measureSceneCompileSeconds(),
                                       diagnostics.uploadBytes + result.residentPathStateBytes(),
                                       result.fullGpuPathLoopUnavailable());
    }

    void annotateCompiledDiffusePathLoopResolve(benchmark::State& state,
                                                const TracingAccumulationLayout& layout,
                                                const TracingAccumulationDiagnostics& diagnostics,
                                                const GpuDiffusePathLoopResult& result,
                                                double measuredSeconds) const {
      state.SetLabel(name + "/compiled_diffuse_path_loop_resolve/" + diagnostics.backend + "/" +
                     diagnostics.residency);
      state.counters["compiled_path_loop_resolved_paths"] =
        static_cast<double>(result.removedPathCount());
      state.counters["compiled_path_loop_resolve_width"] = static_cast<double>(layout.width);
      state.counters["compiled_path_loop_resolve_height"] = static_cast<double>(layout.height);
      state.counters["tracing_render_seconds"] = measuredSeconds;
      state.counters["tracing_upload_readback_seconds"] = 0.0;
      state.counters["tracing_upload_seconds"] = 0.0;
      state.counters["tracing_kernel_seconds"] = measuredSeconds;
      state.counters["tracing_readback_seconds"] = 0.0;
      state.counters["tracing_scene_compile_seconds"] = measureSceneCompileSeconds();
      state.counters["tracing_rays_per_second"] = 0.0;
      state.counters["tracing_resident_bytes"] = static_cast<double>(diagnostics.residentBytes);
      state.counters["tracing_fallback_rate"] = 1.0;
      state.counters["tracing_accumulation_resident_bytes"] =
        static_cast<double>(diagnostics.residentBytes);
      state.counters["tracing_accumulation_color_sum_bytes"] =
        static_cast<double>(layout.colorSumBytes());
      state.counters["tracing_accumulation_sample_count_bytes"] =
        static_cast<double>(layout.sampleCountBytes());
      state.counters["tracing_accumulation_readback_bytes"] =
        static_cast<double>(diagnostics.readbackBytes);
      state.counters["tracing_accumulation_clear_operations"] =
        static_cast<double>(diagnostics.clearOperations);
      state.counters["tracing_accumulation_add_operations"] =
        static_cast<double>(diagnostics.addOperations);
      state.counters["tracing_accumulation_added_samples"] =
        static_cast<double>(diagnostics.addedSamples);
      state.counters["tracing_accumulation_resolve_operations"] =
        static_cast<double>(diagnostics.resolveOperations);
      state.counters["tracing_accumulation_readback_operations"] =
        static_cast<double>(diagnostics.readbackOperations);
      state.counters["tracing_resolve_samples_per_second"] =
        measuredSeconds > 0.0 ? static_cast<double>(diagnostics.addedSamples) / measuredSeconds
                              : 0.0;
    }

    [[nodiscard]] double measureSceneCompileSeconds(std::uint64_t* uploadBytes = nullptr) const {
      const auto start = std::chrono::steady_clock::now();
      const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(*scene);
      const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
      const auto stop = std::chrono::steady_clock::now();
      if (uploadBytes) {
        *uploadBytes = buffers.uploadByteCount();
      }
      benchmark::DoNotOptimize(compiled.primitives().size());
      benchmark::DoNotOptimize(buffers.uploadByteCount());
      return std::chrono::duration<double>(stop - start).count();
    }

    void annotateComparableTracingMetrics(benchmark::State& state, std::uint64_t rayCount,
                                          double measuredSeconds,
                                          const WavefrontIntersectionQueryTiming& timing,
                                          double sceneCompileSeconds, std::uint64_t residentBytes,
                                          bool fallbackActive) const {
      const double raysPerSecond =
        measuredSeconds > 0.0 ? static_cast<double>(rayCount) / measuredSeconds : 0.0;

      state.counters["tracing_render_seconds"] = measuredSeconds;
      state.counters["tracing_upload_readback_seconds"] =
        timing.uploadSeconds + timing.readbackSeconds;
      state.counters["tracing_upload_seconds"] = timing.uploadSeconds;
      state.counters["tracing_kernel_seconds"] = timing.kernelSeconds;
      state.counters["tracing_readback_seconds"] = timing.readbackSeconds;
      state.counters["tracing_scene_compile_seconds"] = sceneCompileSeconds;
      state.counters["tracing_rays_per_second"] = raysPerSecond;
      state.counters["tracing_resident_bytes"] = static_cast<double>(residentBytes);
      state.counters["tracing_fallback_rate"] = fallbackActive ? 1.0 : 0.0;
    }

    void annotateBackendWorkload(
      benchmark::State& state, const WavefrontIntersectionBackend& backend,
      const WavefrontIntersectionQueryTiming& timing,
      const WavefrontIntersectionBackendSelectionContext& context, std::size_t closestHitRayCount,
      std::size_t anyHitRayCount, std::uint64_t closestHitPackedRayBytes = 0,
      std::uint64_t anyHitPackedRayBytes = 0, std::uint64_t closestHitHostPackedRayBytes = 0,
      std::uint64_t anyHitHostPackedRayBytes = 0, std::uint64_t closestHitHostQueryBytes = 0,
      std::uint64_t anyHitHostQueryBytes = 0, std::uint64_t closestHitStateHandleBytes = 0,
      std::uint64_t anyHitStateHandleBytes = 0, double measuredSeconds = 0.0) const {
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
      state.counters["queries"] =
        static_cast<double>((closestHitRayCount > 0 ? 1u : 0u) + (anyHitRayCount > 0 ? 1u : 0u));
      state.counters["closest_hit_queries"] = closestHitRayCount > 0 ? 1.0 : 0.0;
      state.counters["any_hit_queries"] = anyHitRayCount > 0 ? 1.0 : 0.0;
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
      state.counters["upload_seconds"] = timing.uploadSeconds;
      state.counters["kernel_seconds"] = timing.kernelSeconds;
      state.counters["readback_seconds"] = timing.readbackSeconds;
      state.counters["upload_readback_seconds"] = timing.uploadSeconds + timing.readbackSeconds;
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
      state.counters["frontier_host_packed_ray_bytes"] =
        static_cast<double>(closestHitHostPackedRayBytes + anyHitHostPackedRayBytes);
      state.counters["closest_hit_frontier_host_packed_ray_bytes"] =
        static_cast<double>(closestHitHostPackedRayBytes);
      state.counters["any_hit_frontier_host_packed_ray_bytes"] =
        static_cast<double>(anyHitHostPackedRayBytes);
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
      annotateBackendCapabilityCounters(state, backend);
      const double iterations = static_cast<double>(std::max<std::int64_t>(1, state.iterations()));
      WavefrontIntersectionQueryTiming averageTiming = timing;
      averageTiming.uploadSeconds /= iterations;
      averageTiming.kernelSeconds /= iterations;
      averageTiming.readbackSeconds /= iterations;
      const double averageMeasuredSeconds = measuredSeconds > 0.0 ? measuredSeconds / iterations
                                                                  : averageTiming.uploadSeconds +
                                                                      averageTiming.kernelSeconds +
                                                                      averageTiming.readbackSeconds;
      annotateComparableTracingMetrics(
        state, totalRayCount, averageMeasuredSeconds, averageTiming, measureSceneCompileSeconds(),
        diagnostics.uploadBytes + closestHitPackedRayBytes + anyHitPackedRayBytes +
          closestHitStateHandleBytes + anyHitStateHandleBytes,
        std::string(backend.availability()) == "fallback" || !timing.fallbackReason.empty());
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
      state.counters["tracing_scene_compile_seconds"] = measureSceneCompileSeconds();
      state.counters["tracing_resident_bytes"] = static_cast<double>(buffers.uploadByteCount());
      state.counters["tracing_fallback_rate"] =
        compiled.unsupportedPrimitives().empty() ? 0.0 : 1.0;
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

    void annotateBackendCapabilityCounters(benchmark::State& state,
                                           const WavefrontIntersectionBackend& backend) const {
      const bool residentFrontiersSupported = backend.supportsResidentFrontiers();
      const bool gpuFrontierCompactionSupported = backend.supportsGpuFrontierCompaction();
      const bool preparedRayBatchCompactionSupported = backend.supportsPreparedRayBatchCompaction();
      const bool residentDirectLightBatchesSupported = backend.supportsResidentDirectLightBatches();

      state.counters["resident_frontiers_supported"] = residentFrontiersSupported ? 1.0 : 0.0;
      state.counters["gpu_frontier_compaction_supported"] =
        gpuFrontierCompactionSupported ? 1.0 : 0.0;
      state.counters["prepared_ray_batch_compaction_supported"] =
        preparedRayBatchCompactionSupported ? 1.0 : 0.0;
      state.counters["resident_direct_light_batches_supported"] =
        residentDirectLightBatchesSupported ? 1.0 : 0.0;

      state.counters["gpu_frontier_compaction_unavailable"] =
        gpuFrontierCompactionSupported ? 0.0 : 1.0;
      state.counters["gpu_frontier_compaction_unavailable_host_path_state"] =
        !gpuFrontierCompactionSupported && preparedRayBatchCompactionSupported ? 1.0 : 0.0;
      state.counters["gpu_frontier_compaction_unavailable_missing_prepared_ray_batch"] =
        !gpuFrontierCompactionSupported && !preparedRayBatchCompactionSupported ? 1.0 : 0.0;

      state.counters["resident_direct_light_batches_unavailable"] =
        residentDirectLightBatchesSupported ? 0.0 : 1.0;
      state.counters["resident_direct_light_batches_unavailable_host_shading"] =
        !residentDirectLightBatchesSupported && residentFrontiersSupported ? 1.0 : 0.0;
      state.counters["resident_direct_light_batches_unavailable_missing_resident_frontiers"] =
        !residentDirectLightBatchesSupported && !residentFrontiersSupported ? 1.0 : 0.0;
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

  std::shared_ptr<MatteMaterial> matte(const Colord& color) {
    return std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(color));
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

  std::shared_ptr<Scene> makeVisibilityHeavySupportedScene() {
    auto scene = std::make_shared<Scene>();

    constexpr int occluderRows = 24;
    constexpr int occluderColumns = 18;
    for (int row = 0; row != occluderRows; ++row) {
      for (int column = 0; column != occluderColumns; ++column) {
        const double x = (column - occluderColumns / 2) * 0.7 + ((row % 2) == 0 ? 0.0 : 0.25);
        const double y = -1.0 + (row % 6) * 0.38;
        const double z = 3.0 + row * 0.62;
        scene->add(std::make_shared<Sphere>(Vector3d(x, y, z), 0.22));
      }
    }

    scene->add(std::make_shared<Rectangle>(Vector3d(-8.0, -1.35, 18.5), Vector3d::right() * 16.0,
                                           Vector3d::up() * 5.0));
    return scene;
  }

  std::shared_ptr<Scene> makeIndirectDiffuseSupportedScene() {
    auto scene = std::make_shared<Scene>();
    scene->setBackground(Colord(0.02, 0.03, 0.05));
    scene->setEnvironmentRadiance(Colord(0.04, 0.05, 0.07));
    scene->addLight(std::make_shared<PointLight>(Vector3d(0.0, 1.6, -0.35), Colord(4.0, 3.6, 3.1)));

    auto wall = std::make_shared<Rectangle>(Vector3d(-2.0, -1.0, -0.5), Vector3d::up() * 3.0,
                                            Vector3d::forward() * 4.0);
    wall->setMaterial(matte(Colord(0.7, 0.62, 0.55)));
    scene->add(wall);

    auto floor = std::make_shared<Rectangle>(Vector3d(-2.0, -1.0, -0.5), Vector3d::forward() * 4.0,
                                             Vector3d::right() * 4.0);
    floor->setMaterial(matte(Colord(0.55, 0.6, 0.68)));
    scene->add(floor);

    auto sphere = std::make_shared<Sphere>(Vector3d(0.65, -0.35, 1.0), 0.45);
    sphere->setMaterial(matte(Colord(0.85, 0.2, 0.18)));
    scene->add(sphere);
    return scene;
  }

  std::shared_ptr<Scene> makeUnsupportedMixedScene() {
    auto scene = makeMeshHeavySupportedScene();
    core::Polyline polyline;
    polyline.addPoint(Vector3d(-1.5, 0.4, 4.0));
    polyline.addPoint(Vector3d(0.0, 1.0, 5.0));
    polyline.addPoint(Vector3d(1.5, 0.4, 6.0));
    scene->add(std::make_shared<Curve>(polyline, 0.1));
    return scene;
  }

  const std::vector<Workload>& workloads() {
    static const std::vector<Workload> all{
      {"small_supported", makeSmallSupportedScene()},
      {"mesh_heavy_supported", makeMeshHeavySupportedScene()},
      {"visibility_heavy_supported", makeVisibilityHeavySupportedScene()},
      {"indirect_diffuse_supported", makeIndirectDiffuseSupportedScene()},
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

  std::vector<Rayd> generateDiffusePathLoopRays(std::int64_t count) {
    std::mt19937 rng(4321);
    std::uniform_real_distribution<double> originX(-1.75, 1.75);
    std::uniform_real_distribution<double> targetX(-1.6, 1.6);
    std::uniform_real_distribution<double> targetY(-0.95, 0.75);
    std::uniform_real_distribution<double> targetZ(-0.15, 2.8);

    std::vector<Rayd> rays;
    rays.reserve(static_cast<std::size_t>(count));
    for (std::int64_t index = 0; index != count; ++index) {
      const Vector3d origin(originX(rng), 0.85, -4.0);
      const Vector3d target(targetX(rng), targetY(rng), targetZ(rng));
      rays.emplace_back(origin, (target - origin).normalized());
    }
    return rays;
  }

  std::vector<GpuDiffusePathStateRecord> generateDiffusePathStates(std::int64_t count) {
    const std::vector<Rayd> rays = generateDiffusePathLoopRays(count);
    GpuIntersectionScenePacker packer;
    std::vector<GpuDiffusePathStateRecord> paths;
    paths.reserve(rays.size());
    for (std::size_t index = 0; index != rays.size(); ++index) {
      GpuDiffusePathStateRecord path = makeActiveGpuDiffusePathState();
      path.ray = packer.packRay(rays[index], static_cast<std::uint32_t>(index), 0.0,
                                std::numeric_limits<double>::infinity());
      path.pixelIndex = static_cast<std::uint32_t>(index);
      path.primarySampleIndex = 0;
      path.sampleSeed = 0x600DF00Du;
      paths.push_back(path);
    }
    return paths;
  }

  TracingAccumulationLayout accumulationLayoutForPathCount(std::int64_t count) {
    const auto positiveCount = static_cast<double>(std::max<std::int64_t>(1, count));
    const int side = static_cast<int>(std::ceil(std::sqrt(positiveCount)));
    return TracingAccumulationLayout::image(side, side);
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
                     std::size_t readbackRecordSize, double measuredSeconds,
                     double kernelSeconds = 0.0) {
    state.SetLabel(workload.name);
    state.counters["rays"] = static_cast<double>(rayCount);
    state.counters["scene_upload_bytes"] = static_cast<double>(buffers.uploadByteCount());
    state.counters["ray_upload_bytes"] = static_cast<double>(rayCount * sizeof(GpuIntersectionRay));
    state.counters["readback_bytes"] = static_cast<double>(rayCount * readbackRecordSize);
    WavefrontIntersectionQueryTiming timing;
    timing.kernelSeconds = kernelSeconds;
    workload.annotateComparableTracingMetrics(
      state, static_cast<std::uint64_t>(rayCount), measuredSeconds, timing,
      workload.measureSceneCompileSeconds(), buffers.uploadByteCount(), false);
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
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      std::size_t hits = 0;
      for (const Rayd& ray : rays) {
        State traceState;
        HitPointInterval hitPoints;
        if (CpuWavefrontIntersectionBackend::instance().intersectClosest(*workload.scene, ray,
                                                                         hitPoints, traceState)) {
          ++hits;
        }
      }
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();
      benchmark::DoNotOptimize(hits);
    }

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(*workload.scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    const double averageSeconds =
      measuredSeconds / static_cast<double>(std::max<std::int64_t>(1, state.iterations()));
    annotateQuery(state, workload, buffers, rays.size(), sizeof(GpuIntersectionHitRecord),
                  averageSeconds, averageSeconds);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(rays.size()));
  }

  void bm_runtimeCpuAnyHit(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::vector<Rayd> rays = generateRays(state.range(1));
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      std::size_t hits = 0;
      for (const Rayd& ray : rays) {
        State traceState;
        if (CpuWavefrontIntersectionBackend::instance().intersectAny(*workload.scene, ray, 40.0,
                                                                     traceState)) {
          ++hits;
        }
      }
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();
      benchmark::DoNotOptimize(hits);
    }

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(*workload.scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    const double averageSeconds =
      measuredSeconds / static_cast<double>(std::max<std::int64_t>(1, state.iterations()));
    annotateQuery(state, workload, buffers, rays.size(), sizeof(GpuIntersectionOcclusionRecord),
                  averageSeconds, averageSeconds);
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
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      const std::vector<GpuIntersectionHitRecord> hits =
        GpuIntersectionIntersector().intersectClosest(buffers, packedRays);
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();
      benchmark::DoNotOptimize(hits.size());
    }

    const double averageSeconds =
      measuredSeconds / static_cast<double>(std::max<std::int64_t>(1, state.iterations()));
    annotateQuery(state, workload, buffers, packedRays.size(), sizeof(GpuIntersectionHitRecord),
                  averageSeconds, averageSeconds);
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
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      std::size_t hits = 0;
      for (const GpuIntersectionRay& ray : packedRays) {
        if (GpuIntersectionIntersector().intersectAny(buffers, ray)) {
          ++hits;
        }
      }
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();
      benchmark::DoNotOptimize(hits);
    }

    const double averageSeconds =
      measuredSeconds / static_cast<double>(std::max<std::int64_t>(1, state.iterations()));
    annotateQuery(state, workload, buffers, packedRays.size(),
                  sizeof(GpuIntersectionOcclusionRecord), averageSeconds, averageSeconds);
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
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      WavefrontIntersectionQueryTiming queryTiming;
      const std::vector<WavefrontClosestHitResult> hits =
        backend->intersectClosestFrontier(*workload.scene, *frontier, &queryTiming);
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();
      timing.add(queryTiming);
      benchmark::DoNotOptimize(hits.size());
    }

    workload.annotateBackendWorkload(state, *backend, timing, context, frontier->rayCount(), 0,
                                     frontier->packedRayBytes(), 0, frontier->hostPackedRayBytes(),
                                     0, frontier->hostQueryBytes(), 0, frontier->stateHandleBytes(),
                                     0, measuredSeconds);
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
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      WavefrontIntersectionQueryTiming queryTiming;
      const WavefrontOcclusionFlags occluded =
        backend->intersectAnyFrontier(*workload.scene, *frontier, &queryTiming);
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();
      timing.add(queryTiming);
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), 1U));
    }

    workload.annotateBackendWorkload(state, *backend, timing, context, 0, frontier->rayCount(), 0,
                                     frontier->packedRayBytes(), 0, frontier->hostPackedRayBytes(),
                                     0, frontier->hostQueryBytes(), 0, frontier->stateHandleBytes(),
                                     measuredSeconds);
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
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      WavefrontIntersectionQueryTiming closestTiming;
      const std::vector<WavefrontClosestHitResult> hits =
        backend->intersectClosestFrontier(*workload.scene, *closestFrontier, &closestTiming);
      timing.add(closestTiming);

      WavefrontIntersectionQueryTiming anyTiming;
      const WavefrontOcclusionFlags occluded =
        backend->intersectAnyFrontier(*workload.scene, *anyFrontier, &anyTiming);
      timing.add(anyTiming);
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();

      benchmark::DoNotOptimize(hits.size());
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), 1U));
    }

    workload.annotateBackendWorkload(
      state, *backend, timing, context, closestFrontier->rayCount(), anyFrontier->rayCount(),
      closestFrontier->packedRayBytes(), anyFrontier->packedRayBytes(),
      closestFrontier->hostPackedRayBytes(), anyFrontier->hostPackedRayBytes(),
      closestFrontier->hostQueryBytes(), anyFrontier->hostQueryBytes(),
      closestFrontier->stateHandleBytes(), anyFrontier->stateHandleBytes(), measuredSeconds);
    state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(closestFrontier->rayCount() + anyFrontier->rayCount()));
  }

  void bm_intersectionServiceClosestHitBatch(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::vector<Rayd> rays = generateRays(state.range(1));
    const WavefrontIntersectionBackendSelectionContext context =
      workload.selectionContext(rays.size(), 0);
    IntersectionService service(*workload.scene, WavefrontIntersectionBackendChoice::automatic(),
                                context);
    ClosestQueryBatch batch(rays);
    WavefrontIntersectionQueryTiming timing;
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      const std::vector<WavefrontClosestHitResult> hits = service.closestHits(batch.queries);
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();
      timing.add(service.diagnostics().lastClosestHitTiming);
      benchmark::DoNotOptimize(hits.size());
    }

    const std::unique_ptr<WavefrontClosestHitFrontier> frontier =
      service.backend().createClosestHitFrontier(batch.queries);
    workload.annotateBackendWorkload(
      state, service.backend(), timing, context, batch.queries.size(), 0,
      frontier ? frontier->packedRayBytes() : 0, 0, frontier ? frontier->hostPackedRayBytes() : 0,
      0, frontier ? frontier->hostQueryBytes() : 0, 0, frontier ? frontier->stateHandleBytes() : 0,
      0, measuredSeconds);
    state.SetLabel(
      workload.name + "/intersection_service/" + service.diagnostics().requestedBackend + "/" +
      service.diagnostics().selectedBackend + "/" + service.diagnostics().closestHitExecutionPath);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(batch.queries.size()));
  }

  void bm_intersectionServiceAnyHitBatch(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::vector<Rayd> rays = generateRays(state.range(1));
    const WavefrontIntersectionBackendSelectionContext context =
      workload.selectionContext(0, rays.size());
    IntersectionService service(*workload.scene, WavefrontIntersectionBackendChoice::automatic(),
                                context);
    AnyQueryBatch batch(rays, 40.0);
    WavefrontIntersectionQueryTiming timing;
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      const WavefrontOcclusionFlags occluded = service.anyHits(batch.queries);
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();
      timing.add(service.diagnostics().lastAnyHitTiming);
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), 1U));
    }

    const std::unique_ptr<WavefrontAnyHitFrontier> frontier =
      service.backend().createAnyHitFrontier(batch.queries);
    workload.annotateBackendWorkload(
      state, service.backend(), timing, context, 0, batch.queries.size(), 0,
      frontier ? frontier->packedRayBytes() : 0, 0, frontier ? frontier->hostPackedRayBytes() : 0,
      0, frontier ? frontier->hostQueryBytes() : 0, 0, frontier ? frontier->stateHandleBytes() : 0,
      measuredSeconds);
    state.SetLabel(
      workload.name + "/intersection_service/" + service.diagnostics().requestedBackend + "/" +
      service.diagnostics().selectedBackend + "/" + service.diagnostics().anyHitExecutionPath);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(batch.queries.size()));
  }

  void bm_intersectionServiceMixedClosestAndAnyHitBatch(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const std::vector<Rayd> rays = generateRays(state.range(1));
    const WavefrontIntersectionBackendSelectionContext context =
      workload.selectionContext(rays.size(), rays.size());
    IntersectionService service(*workload.scene, WavefrontIntersectionBackendChoice::automatic(),
                                context);
    ClosestQueryBatch closestBatch(rays);
    AnyQueryBatch anyBatch(rays, 40.0);
    WavefrontIntersectionQueryTiming timing;
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      const std::vector<WavefrontClosestHitResult> hits = service.closestHits(closestBatch.queries);
      timing.add(service.diagnostics().lastClosestHitTiming);

      const WavefrontOcclusionFlags occluded = service.anyHits(anyBatch.queries);
      timing.add(service.diagnostics().lastAnyHitTiming);
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();

      benchmark::DoNotOptimize(hits.size());
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), 1U));
    }

    const std::unique_ptr<WavefrontClosestHitFrontier> closestFrontier =
      service.backend().createClosestHitFrontier(closestBatch.queries);
    const std::unique_ptr<WavefrontAnyHitFrontier> anyFrontier =
      service.backend().createAnyHitFrontier(anyBatch.queries);
    workload.annotateBackendWorkload(
      state, service.backend(), timing, context, closestBatch.queries.size(),
      anyBatch.queries.size(), closestFrontier ? closestFrontier->packedRayBytes() : 0,
      anyFrontier ? anyFrontier->packedRayBytes() : 0,
      closestFrontier ? closestFrontier->hostPackedRayBytes() : 0,
      anyFrontier ? anyFrontier->hostPackedRayBytes() : 0,
      closestFrontier ? closestFrontier->hostQueryBytes() : 0,
      anyFrontier ? anyFrontier->hostQueryBytes() : 0,
      closestFrontier ? closestFrontier->stateHandleBytes() : 0,
      anyFrontier ? anyFrontier->stateHandleBytes() : 0, measuredSeconds);
    state.SetLabel(
      workload.name + "/intersection_service/" + service.diagnostics().requestedBackend + "/" +
      service.diagnostics().selectedBackend + "/" + service.diagnostics().executionPath);
    state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(closestBatch.queries.size() + anyBatch.queries.size()));
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
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      WavefrontIntersectionQueryTiming closestTiming;
      const std::vector<WavefrontClosestHitResult> hits =
        backend->intersectClosestFrontier(*workload.scene, *closestFrontier, &closestTiming);
      timing.add(closestTiming);

      WavefrontIntersectionQueryTiming anyTiming;
      const WavefrontOcclusionFlags occluded =
        backend->intersectAnyFrontier(*workload.scene, *anyFrontier, &anyTiming);
      timing.add(anyTiming);
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();

      benchmark::DoNotOptimize(hits.size());
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), 1U));
    }

    workload.annotateBackendWorkload(
      state, *backend, timing, context, closestFrontier->rayCount(), anyFrontier->rayCount(),
      closestFrontier->packedRayBytes(), anyFrontier->packedRayBytes(),
      closestFrontier->hostPackedRayBytes(), anyFrontier->hostPackedRayBytes(),
      closestFrontier->hostQueryBytes(), anyFrontier->hostQueryBytes(),
      closestFrontier->stateHandleBytes(), anyFrontier->stateHandleBytes(), measuredSeconds);
    state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(closestFrontier->rayCount() + anyFrontier->rayCount()));
  }

  void bm_compiledDiffusePathLoopCpuReference(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const GpuTracingSceneCompilation compilation = compileGpuTracingScene(*workload.scene);
    const GpuDiffusePathLoopSupport support =
      gpuDiffusePathLoopSupport(compilation, *workload.scene);
    if (!support.supported) {
      state.SkipWithError(support.reason.c_str());
      return;
    }

    const std::vector<GpuDiffusePathStateRecord> paths = generateDiffusePathStates(state.range(1));
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = static_cast<std::uint32_t>(state.range(2));
    settings.russianRouletteDepth = 3;
    settings.directLightSamples = 1;

    GpuDiffusePathLoopResult result;
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      result = GpuDiffusePathLoop().run(compilation.sections, paths, settings);
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();
      benchmark::DoNotOptimize(result.removedPathCount());
      benchmark::DoNotOptimize(result.stepRecords.size());
    }

    const double averageSeconds =
      measuredSeconds / static_cast<double>(std::max<std::int64_t>(1, state.iterations()));
    workload.annotateCompiledDiffusePathLoop(state, compilation, result, averageSeconds);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(paths.size()));
  }

  void bm_compiledDiffusePathLoopResolveCpuReference(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const GpuTracingSceneCompilation compilation = compileGpuTracingScene(*workload.scene);
    const GpuDiffusePathLoopSupport support =
      gpuDiffusePathLoopSupport(compilation, *workload.scene);
    if (!support.supported) {
      state.SkipWithError(support.reason.c_str());
      return;
    }

    const std::vector<GpuDiffusePathStateRecord> paths = generateDiffusePathStates(state.range(1));
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = static_cast<std::uint32_t>(state.range(2));
    settings.russianRouletteDepth = 3;
    settings.directLightSamples = 1;
    const GpuDiffusePathLoopResult result =
      GpuDiffusePathLoop().run(compilation.sections, paths, settings);
    const TracingAccumulationLayout layout = accumulationLayoutForPathCount(state.range(1));
    Buffer<Colord> target(layout.width, layout.height);

    TracingAccumulationDiagnostics diagnostics;
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      diagnostics = resolveGpuDiffusePathLoopImage(result, layout, target);
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();
      benchmark::DoNotOptimize(diagnostics.addedSamples);
      benchmark::DoNotOptimize(target[0][0]);
    }

    const double averageSeconds =
      measuredSeconds / static_cast<double>(std::max<std::int64_t>(1, state.iterations()));
    workload.annotateCompiledDiffusePathLoopResolve(state, layout, diagnostics, result,
                                                    averageSeconds);
    state.SetItemsProcessed(state.iterations() *
                            static_cast<std::int64_t>(result.removedPathCount()));
  }

#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT) || defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
  void bm_requestedGpuCompiledDiffusePathLoop(benchmark::State& state) {
    const Workload& workload = workloadFor(state);
    const GpuTracingSceneCompilation compilation = compileGpuTracingScene(*workload.scene);
    const GpuDiffusePathLoopSupport support =
      gpuDiffusePathLoopSupport(compilation, *workload.scene);
    if (!support.supported) {
      state.SkipWithError(support.reason.c_str());
      return;
    }

    const std::shared_ptr<const GpuDiffusePathLoopBackend> backend =
      GpuDiffusePathLoopBackend::defaultFullGpuBackendForGpuRequest();
    if (!backend) {
      state.SkipWithError("platform full-GPU path-loop backend is not enabled");
      return;
    }
    if (!backend->fullGpuPathLoopAvailable()) {
      state.SkipWithError(backend->fullGpuPathLoopUnavailableReason());
      return;
    }

    const std::vector<GpuDiffusePathStateRecord> paths = generateDiffusePathStates(state.range(1));
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = static_cast<std::uint32_t>(state.range(2));
    settings.russianRouletteDepth = 3;
    settings.directLightSamples = 1;
    settings.captureDiagnostics = false;

    const GpuDiffusePathLoopBackendSupport backendSupport =
      backend->fullGpuPathLoopSupport(compilation.sections, settings);
    if (!backendSupport.supported) {
      state.SkipWithError(backendSupport.reason.c_str());
      return;
    }

    GpuDiffusePathLoopResult result;
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      result = backend->run(compilation.sections, paths, settings);
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();
      benchmark::DoNotOptimize(result.hasPlatformAccumulation());
      benchmark::DoNotOptimize(result.platformAccumulationColorSums.size());
    }

    const double averageSeconds =
      measuredSeconds / static_cast<double>(std::max<std::int64_t>(1, state.iterations()));
    workload.annotateCompiledDiffusePathLoop(state, compilation, result, averageSeconds);
    state.counters["compiled_path_loop_capture_diagnostics"] = 0.0;
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(paths.size()));
  }

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
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      WavefrontIntersectionQueryTiming queryTiming;
      const std::vector<WavefrontClosestHitResult> hits =
        backend->intersectClosestFrontier(*workload.scene, *frontier, &queryTiming);
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();
      timing.add(queryTiming);
      benchmark::DoNotOptimize(hits.size());
    }

    workload.annotateBackendWorkload(state, *backend, timing, context, frontier->rayCount(), 0,
                                     frontier->packedRayBytes(), 0, frontier->hostPackedRayBytes(),
                                     0, frontier->hostQueryBytes(), 0, frontier->stateHandleBytes(),
                                     0, measuredSeconds);
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
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      WavefrontIntersectionQueryTiming queryTiming;
      const WavefrontOcclusionFlags occluded =
        backend->intersectAnyFrontier(*workload.scene, *frontier, &queryTiming);
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();
      timing.add(queryTiming);
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), 1U));
    }

    workload.annotateBackendWorkload(state, *backend, timing, context, 0, frontier->rayCount(), 0,
                                     frontier->packedRayBytes(), 0, frontier->hostPackedRayBytes(),
                                     0, frontier->hostQueryBytes(), 0, frontier->stateHandleBytes(),
                                     measuredSeconds);
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
    double measuredSeconds = 0.0;
    for (auto _ : state) {
      const auto start = std::chrono::steady_clock::now();
      WavefrontIntersectionQueryTiming closestTiming;
      const std::vector<WavefrontClosestHitResult> hits =
        backend->intersectClosestFrontier(*workload.scene, *closestFrontier, &closestTiming);
      timing.add(closestTiming);

      WavefrontIntersectionQueryTiming anyTiming;
      const WavefrontOcclusionFlags occluded =
        backend->intersectAnyFrontier(*workload.scene, *anyFrontier, &anyTiming);
      timing.add(anyTiming);
      const auto stop = std::chrono::steady_clock::now();
      measuredSeconds += std::chrono::duration<double>(stop - start).count();

      benchmark::DoNotOptimize(hits.size());
      benchmark::DoNotOptimize(std::count(occluded.begin(), occluded.end(), 1U));
    }

    workload.annotateBackendWorkload(
      state, *backend, timing, context, closestFrontier->rayCount(), anyFrontier->rayCount(),
      closestFrontier->packedRayBytes(), anyFrontier->packedRayBytes(),
      closestFrontier->hostPackedRayBytes(), anyFrontier->hostPackedRayBytes(),
      closestFrontier->hostQueryBytes(), anyFrontier->hostQueryBytes(),
      closestFrontier->stateHandleBytes(), anyFrontier->stateHandleBytes(), measuredSeconds);
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
                                          std::size_t retainedRayCount,
                                          const WavefrontFrontierCompactionTiming& timing) {
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
    state.counters["frontier_compaction_upload_seconds"] = timing.uploadSeconds;
    state.counters["frontier_compaction_kernel_seconds"] = timing.kernelSeconds;
    state.counters["frontier_compaction_readback_seconds"] = timing.readbackSeconds;
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

    MetalWavefrontRayBatchCompactionResult compaction =
      prepared.compactRaysTimed(*sourceBatch, retainedIndices);
    for (auto _ : state) {
      compaction = prepared.compactRaysTimed(*sourceBatch, retainedIndices);
      benchmark::DoNotOptimize(compaction.rays->rayCount());
    }

    annotatePreparedRayBatchCompaction(state, workload, "metal", packedRays.size(),
                                       retainedIndices.size(), compaction.timing);
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

    VulkanWavefrontRayBatchCompactionResult compaction =
      prepared.compactRaysTimed(*sourceBatch, retainedIndices);
    for (auto _ : state) {
      compaction = prepared.compactRaysTimed(*sourceBatch, retainedIndices);
      benchmark::DoNotOptimize(compaction.rays->rayCount());
    }

    annotatePreparedRayBatchCompaction(state, workload, "vulkan", packedRays.size(),
                                       retainedIndices.size(), compaction.timing);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(packedRays.size()));
  }
#endif
#endif

  void allWorkloads(benchmark::internal::Benchmark* benchmark) {
    for (int workload = 0; workload != 5; ++workload) {
      benchmark->Arg(workload);
    }
  }

  void supportedQueryWorkloads(benchmark::internal::Benchmark* benchmark) {
    for (int workload = 0; workload != 4; ++workload) {
      benchmark->Args({workload, 256});
      benchmark->Args({workload, 65536});
    }
  }

  void unsupportedQueryWorkloads(benchmark::internal::Benchmark* benchmark) {
    benchmark->Args({4, 256});
    benchmark->Args({4, 65536});
  }

  void compiledDiffusePathLoopWorkloads(benchmark::internal::Benchmark* benchmark) {
    benchmark->Args({3, 256, 4});
    benchmark->Args({3, 4096, 4});
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
BENCHMARK(bm_intersectionServiceClosestHitBatch)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_intersectionServiceAnyHitBatch)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_intersectionServiceMixedClosestAndAnyHitBatch)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_autoFrontierCompaction)->Apply(supportedQueryWorkloads);
BENCHMARK(bm_requestedGpuUnsupportedMixedClosestAndAnyHitBatch)->Apply(unsupportedQueryWorkloads);
BENCHMARK(bm_compiledDiffusePathLoopCpuReference)->Apply(compiledDiffusePathLoopWorkloads);
BENCHMARK(bm_compiledDiffusePathLoopResolveCpuReference)->Apply(compiledDiffusePathLoopWorkloads);
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT) || defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
BENCHMARK(bm_requestedGpuCompiledDiffusePathLoop)->Apply(compiledDiffusePathLoopWorkloads);
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
