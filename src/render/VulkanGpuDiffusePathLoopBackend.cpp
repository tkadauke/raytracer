#include "render/VulkanGpuDiffusePathLoopBackend.h"

#include "render/GpuDiffusePathLoopSceneSupport.h"
#include "render/VulkanGpuDiffusePathLoopKernel.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace render {
  namespace {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    constexpr const char* kVulkanBackendDisplayName = "Vulkan";
    constexpr const char* kVulkanPlatformName = "vulkan";
    constexpr const char* kVulkanPathStateResidency = "vulkan_host_visible_diffuse_path_state";
    constexpr const char* kVulkanAccumulationBackend = "vulkan_diffuse_path_loop";
    constexpr const char* kVulkanAccumulationResidency = "vulkan_accumulation_buffer";

    [[nodiscard]] GpuDiffusePathLoopPlatformResult
    platformResultFrom(VulkanGpuDiffusePathLoopKernelResult&& result) {
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
    makeVulkanLoopResult(std::uint64_t initialPathCount, const GpuDiffusePathLoopSettings& settings,
                         VulkanGpuDiffusePathLoopKernelResult&& vulkanResult) {
      return makePlatformGpuDiffusePathLoopResult(
        initialPathCount, settings, platformResultFrom(std::move(vulkanResult)),
        kVulkanBackendDisplayName, kVulkanPlatformName, kVulkanPathStateResidency,
        kVulkanAccumulationBackend, kVulkanAccumulationResidency);
    }
#endif
  }

  std::shared_ptr<const VulkanGpuDiffusePathLoopBackend>
  VulkanGpuDiffusePathLoopBackend::sharedInstance() {
    static const std::shared_ptr<const VulkanGpuDiffusePathLoopBackend> instance =
      std::make_shared<VulkanGpuDiffusePathLoopBackend>();
    return instance;
  }

  const char* VulkanGpuDiffusePathLoopBackend::name() const {
    return "vulkan_diffuse_path_loop";
  }

  bool VulkanGpuDiffusePathLoopBackend::fullGpuPathLoopAvailable() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanGpuDiffusePathLoopKernel().launchPathAvailable();
#else
    return false;
#endif
  }

  const char* VulkanGpuDiffusePathLoopBackend::fullGpuPathLoopUnavailableReason() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    static thread_local std::string reason;
    reason = VulkanGpuDiffusePathLoopKernel().launchPathUnavailableReason();
    if (reason.empty()) {
      reason = "Vulkan diffuse path-loop launch path is available";
    }
    return reason.c_str();
#else
    return "Vulkan diffuse path-loop backend is not enabled in this build";
#endif
  }

  const char* VulkanGpuDiffusePathLoopBackend::platformName() const {
    return "vulkan";
  }

  GpuDiffusePathLoopBackendSupport VulkanGpuDiffusePathLoopBackend::fullGpuPathLoopSupport(
    const GpuTracingSceneSections& scene, const GpuDiffusePathLoopSettings& settings) const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    VulkanGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      return {false, kernel.launchPathUnavailableReason()};
    }
    return GpuDiffusePathLoopSceneSupport().support(
      scene, settings,
      GpuDiffusePathLoopSceneSupportReasons{
        "Vulkan diffuse path-loop backend requires positive max depth",
        "Vulkan diffuse path-loop backend currently supports empty geometry or "
        "triangle-backed MeshPrimitive, Box, and finite-width Curve geometry plus triangle, "
        "sphere, plane, rectangle, disk, open-cylinder, or torus records with static or "
        "linearly moving transforms only",
        "Vulkan diffuse path-loop backend currently supports Matte, Phong finite glossy, "
        "Reflective mirror, Transparent refraction, Emissive, and Portal materials only",
        "Vulkan diffuse path-loop backend currently supports ConstantColor, CheckerBoard texture "
        "graphs, nearest, bilinear, and base-level mipmapped ImageTexture, UVColorTexture, and "
        "bounded Tinted wrapper chains over those textures only",
        "Vulkan diffuse path-loop backend currently supports point, directional, or rectangular "
        "area lights only",
        "Vulkan diffuse path-loop backend requires finite ambient, background, and environment "
        "colors",
        "Vulkan diffuse path-loop backend requires Linear, Reinhard, or ACES display-resolve "
        "tonemapping when resolved display pixels are requested",
        "Vulkan diffuse path-loop backend does not support wavefront convergence yet"});
#else
    (void)scene;
    (void)settings;
    return {false, fullGpuPathLoopUnavailableReason()};
#endif
  }

  GpuDiffusePathLoopResult VulkanGpuDiffusePathLoopBackend::run(
    const GpuTracingSceneSections& scene,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
    const GpuDiffusePathLoopSettings& settings) const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const GpuDiffusePathLoopBackendSupport support = fullGpuPathLoopSupport(scene, settings);
    if (!support.supported) {
      throw std::invalid_argument(support.reason);
    }
    throwIfGpuDiffusePathLoopCancelled(settings);
    const GpuDiffusePathLoopPlatformAccumulationPlan accumulation =
      platformGpuDiffusePathLoopAccumulationPlanFor(initialPathStates, kVulkanBackendDisplayName);
    GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      scene, initialPathStates, accumulation.layout, settings);
    plan.parameters.accumulationTargetMode = accumulation.targetMode;
    VulkanGpuDiffusePathLoopKernelResult vulkanResult =
      VulkanGpuDiffusePathLoopKernel().runWavefrontPathLoop(plan, initialPathStates,
                                                            settings.capturePlatformAccumulation,
                                                            settings.captureResolvedDisplay);
    return makeVulkanLoopResult(static_cast<std::uint64_t>(initialPathStates.size()), settings,
                                std::move(vulkanResult));
#else
    (void)scene;
    (void)initialPathStates;
    (void)settings;
    throw std::runtime_error(fullGpuPathLoopUnavailableReason());
#endif
  }

  GpuDiffusePathLoopResult VulkanGpuDiffusePathLoopBackend::run(
    const GpuTracingSceneSections& scene,
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const GpuDiffusePathLoopSettings& settings) const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const GpuDiffusePathLoopBackendSupport support = fullGpuPathLoopSupport(scene, settings);
    if (!support.supported) {
      throw std::invalid_argument(support.reason);
    }
    throwIfGpuDiffusePathLoopCancelled(settings);
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates =
      primaryPathGeneration.pathStates;
    const GpuDiffusePathLoopPlatformAccumulationPlan accumulation =
      platformGpuDiffusePathLoopAccumulationPlanFor(primaryPathGeneration, settings,
                                                    kVulkanBackendDisplayName);
    const std::vector<GpuDiffusePrimaryPathSampleChunk> chunks =
      gpuDiffusePrimarySampleChunksFor(primaryPathGeneration, settings);
    if (!chunks.empty()) {
      GpuDiffusePathLoopLaunchPlanner planner;
      GpuDiffusePathLoopLaunchPlan fullPlan =
        planner.plan(scene, primaryPathGeneration, accumulation.layout, settings);
      fullPlan.parameters.accumulationTargetMode = accumulation.targetMode;

      VulkanGpuDiffusePathLoopKernel kernel;
      GpuDiffusePathLoopPlatformResult mergedPlatformResult;
      for (const GpuDiffusePrimaryPathSampleChunk& chunk : chunks) {
        throwIfGpuDiffusePathLoopCancelled(settings);
        GpuDiffusePathLoopLaunchPlan chunkPlan =
          planner.plan(scene, chunk.primaryPathGeneration, accumulation.layout, settings);
        chunkPlan.parameters.accumulationTargetMode = accumulation.targetMode;
        const bool captureChunkResolvedDisplay =
          shouldCaptureGpuDiffusePathLoopChunkResolvedDisplay(settings, chunk);
        VulkanGpuDiffusePathLoopKernelResult vulkanResult = kernel.runWavefrontPathLoop(
          chunkPlan, initialPathStates, chunk.finalChunk && settings.capturePlatformAccumulation,
          captureChunkResolvedDisplay, chunk.firstChunk);
        GpuDiffusePathLoopPlatformResult chunkPlatformResult =
          platformResultFrom(std::move(vulkanResult));
        notifyGpuDiffusePathLoopChunkProgress(settings, primaryPathGeneration, chunk,
                                              chunkPlatformResult);
        throwIfGpuDiffusePathLoopCancelled(settings);
        mergePlatformGpuDiffusePathLoopChunkResult(mergedPlatformResult,
                                                   std::move(chunkPlatformResult));
      }
      mergedPlatformResult.echoedParameters = fullPlan.parameters;
      GpuDiffusePathLoopResult result = makePlatformGpuDiffusePathLoopResult(
        fullPlan.parameters.initialPathCount, settings, std::move(mergedPlatformResult),
        kVulkanBackendDisplayName, kVulkanPlatformName, kVulkanPathStateResidency,
        kVulkanAccumulationBackend, kVulkanAccumulationResidency);
      result.roundTrips = chunks.size();
      return result;
    }
    GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      scene, primaryPathGeneration, accumulation.layout, settings);
    plan.parameters.accumulationTargetMode = accumulation.targetMode;
    VulkanGpuDiffusePathLoopKernelResult vulkanResult =
      VulkanGpuDiffusePathLoopKernel().runWavefrontPathLoop(plan, initialPathStates,
                                                            settings.capturePlatformAccumulation,
                                                            settings.captureResolvedDisplay);
    return makeVulkanLoopResult(plan.parameters.initialPathCount, settings,
                                std::move(vulkanResult));
#else
    (void)scene;
    (void)primaryPathGeneration;
    (void)settings;
    throw std::runtime_error(fullGpuPathLoopUnavailableReason());
#endif
  }
}
