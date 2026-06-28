#include "render/MetalGpuDiffusePathLoopBackend.h"

#include "render/GpuDiffusePathLoopSceneSupport.h"
#include "render/MetalGpuDiffusePathLoopKernel.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace render {
  namespace {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    constexpr const char* kMetalBackendDisplayName = "Metal";
    constexpr const char* kMetalPlatformName = "metal";
    constexpr const char* kMetalPathStateResidency = "metal_shared_diffuse_path_state";
    constexpr const char* kMetalAccumulationBackend = "metal_diffuse_path_loop";
    constexpr const char* kMetalAccumulationResidency = "metal_accumulation_buffer";

    [[nodiscard]] GpuDiffusePathLoopPlatformResult
    platformResultFrom(MetalGpuDiffusePathLoopKernelResult&& result) {
      GpuDiffusePathLoopPlatformResult platform;
      platform.echoedParameters = result.echoedParameters;
      platform.resolvedPathStates = std::move(result.resolvedPathStates);
      platform.nextPathStates = std::move(result.nextPathStates);
      platform.stepRecords = std::move(result.stepRecords);
      platform.denoiserFeatureRecords = std::move(result.denoiserFeatureRecords);
      platform.retainedPathCount = result.retainedPathCount;
      platform.activePathCountsPerDepth = std::move(result.activePathCountsPerDepth);
      platform.accumulationColorSums = std::move(result.accumulationColorSums);
      platform.accumulationSampleCounts = std::move(result.accumulationSampleCounts);
      platform.resolvedDisplayPixels = std::move(result.resolvedDisplayPixels);
      platform.resolvedDisplayReadbacks = platform.resolvedDisplayPixels.empty() ? 0u : 1u;
      platform.executionPath = std::move(result.executionPath);
      platform.schedule = std::move(result.schedule);
      platform.pathStateResidency = std::move(result.pathStateResidency);
      platform.retainedFrontierDispatchesIndirect = result.retainedFrontierDispatchesIndirect;
      platform.sceneUploadCacheHit = result.sceneUploadCacheHit;
      platform.sceneUploadBytesWritten = result.sceneUploadBytesWritten;
      platform.uploadWorkerSeconds = result.uploadWorkerSeconds;
      platform.kernelWorkerSeconds = result.kernelWorkerSeconds;
      platform.readbackWorkerSeconds = result.readbackWorkerSeconds;
      return platform;
    }

    [[nodiscard]] GpuDiffusePathLoopResult
    makeMetalLoopResult(std::uint64_t initialPathCount, const GpuDiffusePathLoopSettings& settings,
                        MetalGpuDiffusePathLoopKernelResult&& metalResult) {
      return makePlatformGpuDiffusePathLoopResult(
        initialPathCount, settings, platformResultFrom(std::move(metalResult)),
        kMetalBackendDisplayName, kMetalPlatformName, kMetalPathStateResidency,
        kMetalAccumulationBackend, kMetalAccumulationResidency);
    }
#endif
  }

  std::shared_ptr<const MetalGpuDiffusePathLoopBackend>
  MetalGpuDiffusePathLoopBackend::sharedInstance() {
    static const std::shared_ptr<const MetalGpuDiffusePathLoopBackend> instance =
      std::make_shared<MetalGpuDiffusePathLoopBackend>();
    return instance;
  }

  const char* MetalGpuDiffusePathLoopBackend::name() const {
    return "metal_diffuse_path_loop";
  }

  bool MetalGpuDiffusePathLoopBackend::fullGpuPathLoopAvailable() const {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    return MetalGpuDiffusePathLoopKernel().launchPathAvailable();
#else
    return false;
#endif
  }

  const char* MetalGpuDiffusePathLoopBackend::fullGpuPathLoopUnavailableReason() const {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    static thread_local std::string reason;
    reason = MetalGpuDiffusePathLoopKernel().launchPathUnavailableReason();
    if (reason.empty()) {
      reason = "Metal diffuse path-loop launch path is available";
    }
    return reason.c_str();
#else
    return "Metal diffuse path-loop backend is not enabled in this build";
#endif
  }

  const char* MetalGpuDiffusePathLoopBackend::platformName() const {
    return "metal";
  }

  GpuDiffusePathLoopBackendSupport MetalGpuDiffusePathLoopBackend::fullGpuPathLoopSupport(
    const GpuTracingSceneSections& scene, const GpuDiffusePathLoopSettings& settings) const {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      return {false, kernel.launchPathUnavailableReason()};
    }
    return GpuDiffusePathLoopSceneSupport().support(
      scene, settings,
      GpuDiffusePathLoopSceneSupportReasons{
        "Metal diffuse path-loop backend requires positive max depth",
        "Metal diffuse path-loop backend currently supports empty geometry or "
        "triangle-backed MeshPrimitive, Box, and finite-width Curve geometry plus "
        "triangle/sphere/plane/rectangle/disk/open-cylinder/torus records with static "
        "transforms only",
        "Metal diffuse path-loop backend currently supports Matte, Phong finite glossy, "
        "Reflective mirror, Transparent refraction, Emissive, and Portal materials only",
        "Metal diffuse path-loop backend currently supports ConstantColor, CheckerBoard texture "
        "graphs, nearest, bilinear, and base-level mipmapped ImageTexture, UVColorTexture, and "
        "bounded Tinted wrapper chains over those textures only",
        "Metal diffuse path-loop backend currently supports point, directional, and rectangular "
        "area lights only",
        "Metal diffuse path-loop backend requires finite ambient, background, and environment "
        "colors",
        "Metal diffuse path-loop backend requires Linear, Reinhard, or ACES display-resolve "
        "tonemapping when resolved display pixels are requested",
        "Metal diffuse path-loop backend does not support wavefront convergence yet"});
#else
    (void)scene;
    (void)settings;
    return {false, fullGpuPathLoopUnavailableReason()};
#endif
  }

  GpuDiffusePathLoopResult MetalGpuDiffusePathLoopBackend::run(
    const GpuTracingSceneSections& scene,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
    const GpuDiffusePathLoopSettings& settings) const {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const GpuDiffusePathLoopBackendSupport support = fullGpuPathLoopSupport(scene, settings);
    if (!support.supported) {
      throw std::invalid_argument(support.reason);
    }
    throwIfGpuDiffusePathLoopCancelled(settings);
    const GpuDiffusePathLoopPlatformAccumulationPlan accumulation =
      platformGpuDiffusePathLoopAccumulationPlanFor(initialPathStates, kMetalBackendDisplayName);
    GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      scene, initialPathStates, accumulation.layout, settings);
    plan.parameters.accumulationTargetMode = accumulation.targetMode;
    MetalGpuDiffusePathLoopKernelResult metalResult =
      MetalGpuDiffusePathLoopKernel().runWavefrontPathLoop(plan, initialPathStates,
                                                           settings.capturePlatformAccumulation,
                                                           settings.captureResolvedDisplay);
    return makeMetalLoopResult(static_cast<std::uint64_t>(initialPathStates.size()), settings,
                               std::move(metalResult));
#else
    (void)scene;
    (void)initialPathStates;
    (void)settings;
    throw std::runtime_error(fullGpuPathLoopUnavailableReason());
#endif
  }

  GpuDiffusePathLoopResult MetalGpuDiffusePathLoopBackend::run(
    const GpuTracingSceneSections& scene,
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const GpuDiffusePathLoopSettings& settings) const {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const GpuDiffusePathLoopBackendSupport support = fullGpuPathLoopSupport(scene, settings);
    if (!support.supported) {
      throw std::invalid_argument(support.reason);
    }
    throwIfGpuDiffusePathLoopCancelled(settings);
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates =
      primaryPathGeneration.pathStates;
    const GpuDiffusePathLoopPlatformAccumulationPlan accumulation =
      platformGpuDiffusePathLoopAccumulationPlanFor(primaryPathGeneration, settings,
                                                    kMetalBackendDisplayName);
    const std::vector<GpuDiffusePrimaryPathSampleChunk> chunks =
      gpuDiffusePrimarySampleChunksFor(primaryPathGeneration, settings);
    if (!chunks.empty()) {
      GpuDiffusePathLoopLaunchPlanner planner;
      GpuDiffusePathLoopLaunchPlan fullPlan =
        planner.plan(scene, primaryPathGeneration, accumulation.layout, settings);
      fullPlan.parameters.accumulationTargetMode = accumulation.targetMode;

      MetalGpuDiffusePathLoopKernel kernel;
      GpuDiffusePathLoopPlatformResult mergedPlatformResult;
      for (const GpuDiffusePrimaryPathSampleChunk& chunk : chunks) {
        throwIfGpuDiffusePathLoopCancelled(settings);
        GpuDiffusePathLoopLaunchPlan chunkPlan =
          planner.plan(scene, chunk.primaryPathGeneration, accumulation.layout, settings);
        chunkPlan.parameters.accumulationTargetMode = accumulation.targetMode;
        const bool captureChunkResolvedDisplay =
          shouldCaptureGpuDiffusePathLoopChunkResolvedDisplay(settings, chunk);
        MetalGpuDiffusePathLoopKernelResult metalResult = kernel.runWavefrontPathLoop(
          chunkPlan, initialPathStates, chunk.finalChunk && settings.capturePlatformAccumulation,
          captureChunkResolvedDisplay, chunk.firstChunk);
        GpuDiffusePathLoopPlatformResult chunkPlatformResult =
          platformResultFrom(std::move(metalResult));
        notifyGpuDiffusePathLoopChunkProgress(settings, primaryPathGeneration, chunk,
                                              chunkPlatformResult);
        throwIfGpuDiffusePathLoopCancelled(settings);
        mergePlatformGpuDiffusePathLoopChunkResult(mergedPlatformResult,
                                                   std::move(chunkPlatformResult));
      }
      mergedPlatformResult.echoedParameters = fullPlan.parameters;
      GpuDiffusePathLoopResult result = makePlatformGpuDiffusePathLoopResult(
        fullPlan.parameters.initialPathCount, settings, std::move(mergedPlatformResult),
        kMetalBackendDisplayName, kMetalPlatformName, kMetalPathStateResidency,
        kMetalAccumulationBackend, kMetalAccumulationResidency);
      result.roundTrips = chunks.size();
      return result;
    }
    GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      scene, primaryPathGeneration, accumulation.layout, settings);
    plan.parameters.accumulationTargetMode = accumulation.targetMode;
    MetalGpuDiffusePathLoopKernelResult metalResult =
      MetalGpuDiffusePathLoopKernel().runWavefrontPathLoop(plan, initialPathStates,
                                                           settings.capturePlatformAccumulation,
                                                           settings.captureResolvedDisplay);
    return makeMetalLoopResult(plan.parameters.initialPathCount, settings, std::move(metalResult));
#else
    (void)scene;
    (void)primaryPathGeneration;
    (void)settings;
    throw std::runtime_error(fullGpuPathLoopUnavailableReason());
#endif
  }
}
