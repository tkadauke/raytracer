#include "render/VulkanGpuDiffusePathLoopKernel.h"

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
#include "render/VulkanDiffusePathLoopAdvanceFrontier.generated.h"
#include "render/VulkanDiffusePathLoopClearFrontier.generated.h"
#include "render/VulkanDiffusePathLoopDisplayResolve.generated.h"
#include "render/VulkanDiffusePathLoopInitializeFrontier.generated.h"
#include "render/VulkanDiffusePathLoopPrepareDispatch.generated.h"

#include <vulkan/vulkan.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace render {
  namespace {
    static_assert(sizeof(GpuDiffusePathLoopLaunchParameters) == 352);
    static_assert(alignof(GpuDiffusePathLoopLaunchParameters) == 16);
    static_assert(sizeof(GpuDiffusePathStateRecord) == 160);
    static_assert(alignof(GpuDiffusePathStateRecord) == 16);
    static_assert(sizeof(GpuDiffusePathStepRecord) == 112);
    static_assert(alignof(GpuDiffusePathStepRecord) == 16);
    static_assert(sizeof(GpuDiffusePathDenoiserFeatureRecord) == 64);
    static_assert(alignof(GpuDiffusePathDenoiserFeatureRecord) == 16);
    static_assert(sizeof(GpuTracingEnvironmentRecord) == 32);
    static_assert(alignof(GpuTracingEnvironmentRecord) == 16);

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    constexpr std::uint32_t kDiffusePathLoopLocalSizeX = 64u;
    constexpr std::uint32_t kDiffusePathLoopDescriptorCount = 14u;
    constexpr VkDeviceSize kDispatchIndirectCommandBytes = 3u * sizeof(std::uint32_t);

    std::uint64_t pixelCount(const GpuDiffusePathLoopLaunchParameters& parameters) {
      return static_cast<std::uint64_t>(parameters.imageWidth) *
             static_cast<std::uint64_t>(parameters.imageHeight);
    }

    std::uint64_t displayPixelCount(const GpuDiffusePathLoopLaunchParameters& parameters) {
      if (parameters.accumulationTargetMode == gpuDiffusePathLoopAccumulationTargetPath) {
        return 0u;
      }
      if (parameters.accumulationTargetMode == gpuDiffusePathLoopAccumulationTargetSampleSlot) {
        return parameters.imageWidth;
      }
      return pixelCount(parameters);
    }

    void validatePathLoopPlan(const GpuDiffusePathLoopLaunchPlan& plan,
                              const std::vector<GpuDiffusePathStateRecord>& initialPathStates) {
      if (plan.parameters.layoutVersion != gpuDiffusePathLoopLaunchLayoutVersion) {
        throw std::invalid_argument("Vulkan diffuse path-loop descriptor version mismatch");
      }
      if (plan.parameters.maxDepth == 0u) {
        throw std::invalid_argument("Vulkan diffuse path-loop requires positive max depth");
      }
      const std::size_t launchPathCount =
        static_cast<std::size_t>(plan.parameters.initialPathCount);
      if (initialPathStates.size() != launchPathCount &&
          (!plan.generatesPrimaryPathsOnDevice() || !initialPathStates.empty())) {
        throw std::invalid_argument(
          "Vulkan diffuse path-loop initial path-state count does not match launch descriptor");
      }
      if (plan.sceneUpload.size() != plan.buffers.sceneUploadBytes) {
        throw std::invalid_argument(
          "Vulkan diffuse path-loop scene upload bytes do not match launch descriptor");
      }
      if (plan.parameters.environmentCount != 0u) {
        const std::uint64_t environmentBytes =
          static_cast<std::uint64_t>(plan.parameters.environmentCount) *
          static_cast<std::uint64_t>(sizeof(GpuTracingEnvironmentRecord));
        const std::uint64_t end =
          static_cast<std::uint64_t>(plan.parameters.environmentByteOffset) + environmentBytes;
        if (end > plan.parameters.sceneUploadBytes) {
          throw std::invalid_argument(
            "Vulkan diffuse path-loop environment section exceeds scene upload bytes");
        }
      }
    }

    std::uint32_t
    retainedPathCountFromBuffer(const std::vector<std::uint32_t>& countPrefixedIndices,
                                std::size_t maxPathCount) {
      if (countPrefixedIndices.empty()) {
        return 0u;
      }
      return std::min<std::uint32_t>(countPrefixedIndices.front(),
                                     static_cast<std::uint32_t>(maxPathCount));
    }

    std::vector<std::uint32_t>
    retainedPathIndicesFromBuffer(const std::vector<std::uint32_t>& countPrefixedIndices,
                                  std::size_t maxPathCount) {
      const std::uint32_t count = retainedPathCountFromBuffer(countPrefixedIndices, maxPathCount);
      std::vector<std::uint32_t> result;
      result.reserve(count);
      for (std::uint32_t index = 0; index != count; ++index) {
        result.push_back(countPrefixedIndices[static_cast<std::size_t>(index) + 1u]);
      }
      return result;
    }

    double secondsBetween(std::chrono::steady_clock::time_point start,
                          std::chrono::steady_clock::time_point end) {
      return std::chrono::duration<double>(end - start).count();
    }

    class VulkanDiffusePathLoopRuntime final {
    public:
      VulkanDiffusePathLoopRuntime() {
        try {
          initialize();
        } catch (const std::runtime_error& e) {
          m_unavailableReason = e.what();
          destroy();
        }
      }

      ~VulkanDiffusePathLoopRuntime() {
        destroy();
      }

      VulkanDiffusePathLoopRuntime(const VulkanDiffusePathLoopRuntime&) = delete;
      VulkanDiffusePathLoopRuntime& operator=(const VulkanDiffusePathLoopRuntime&) = delete;

      bool deviceAvailable() const {
        return deviceUnavailableReason().empty();
      }

      std::string deviceUnavailableReason() const {
        return m_unavailableReason;
      }

      bool launchPathAvailable() const {
        return launchPathUnavailableReason().empty();
      }

      std::string launchPathUnavailableReason() const {
        return m_unavailableReason;
      }

      VulkanGpuDiffusePathLoopKernelResult
      runWavefrontPathLoop(const GpuDiffusePathLoopLaunchPlan& plan,
                           const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
                           bool capturePlatformAccumulation, bool captureResolvedDisplay) const {
        std::lock_guard<std::mutex> lock(m_runMutex);
        if (!launchPathAvailable()) {
          throw std::runtime_error(launchPathUnavailableReason());
        }
        validatePathLoopPlan(plan, initialPathStates);
        const std::size_t launchPathCount =
          static_cast<std::size_t>(plan.parameters.initialPathCount);

        const auto uploadStart = std::chrono::steady_clock::now();
        const std::uint64_t pixels = pixelCount(plan.parameters);
        const std::uint64_t resolvedDisplayPixels =
          captureResolvedDisplay ? displayPixelCount(plan.parameters) : 0u;
        if (captureResolvedDisplay && resolvedDisplayPixels == 0u) {
          throw std::invalid_argument(
            "Vulkan diffuse path-loop display resolve requires pixel or sample-slot accumulation");
        }
        const std::uint64_t pathStateStorageBytes =
          launchPathCount * static_cast<std::uint64_t>(sizeof(GpuDiffusePathStateRecord));
        prepareStorageBuffer(0u, sizeof(GpuDiffusePathLoopLaunchParameters), &plan.parameters);
        prepareStorageBuffer(1u, sizeof(GpuDiffusePathLoopLaunchParameters), nullptr);
        const bool sceneUploadCacheHit =
          prepareStorageBufferFromBytesIfChanged(2u, plan.sceneUpload, m_sceneUploadCache);
        if (plan.generatesPrimaryPathsOnDevice()) {
          prepareStorageBuffer(3u, 1u, nullptr);
        } else {
          prepareStorageBufferFromVector(3u, initialPathStates);
        }
        prepareStorageBuffer(4u, static_cast<VkDeviceSize>(pathStateStorageBytes), nullptr);
        prepareStorageBuffer(5u, 1u, nullptr);
        std::vector<std::uint8_t> stepRecordBytes(
          static_cast<std::size_t>(plan.buffers.stepRecordBytes), 0u);
        prepareStorageBufferFromBytes(6u, stepRecordBytes);
        const std::size_t retainedIndexCount =
          static_cast<std::size_t>(plan.buffers.retainedIndexBytes / sizeof(std::uint32_t));
        std::vector<std::uint32_t> retainedIndices(retainedIndexCount, 0u);
        prepareStorageBufferFromVector(7u, retainedIndices);
        std::vector<std::uint8_t> accumulationBytes(
          static_cast<std::size_t>(std::max<std::uint64_t>(1u, plan.buffers.accumulationBytes)),
          0u);
        prepareStorageBufferFromBytes(8u, accumulationBytes);
        std::vector<std::uint8_t> denoiserFeatureBytes(
          static_cast<std::size_t>(
            std::max<std::uint64_t>(1u, plan.buffers.denoiserFeatureRecordBytes)),
          0u);
        prepareStorageBufferFromBytes(9u, denoiserFeatureBytes);
        std::vector<std::uint8_t> activePathCountBytes(
          static_cast<std::size_t>(std::max<std::uint64_t>(1u, plan.buffers.activePathCountBytes)),
          0u);
        prepareStorageBufferFromBytes(10u, activePathCountBytes);
        prepareStorageBuffer(
          11u, resolvedDisplayPixels * static_cast<std::uint64_t>(sizeof(unsigned int)), nullptr);
        prepareStorageBufferFromVector(12u, retainedIndices);
        prepareStorageBuffer(13u, kDispatchIndirectCommandBytes, nullptr,
                             VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        prepareStorageBuffer(14u, kDispatchIndirectCommandBytes, nullptr,
                             VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

        VkDescriptorSet descriptorSetAB = m_descriptorSetAB;
        VkDescriptorSet descriptorSetBA = m_descriptorSetBA;
        std::vector<StorageBuffer> descriptorBuffersAB(
          m_buffers.begin(), m_buffers.begin() + kDiffusePathLoopDescriptorCount);
        updateDescriptorSet(m_device, descriptorSetAB, descriptorBuffersAB);
        std::vector<StorageBuffer> swappedFrontierBuffers = descriptorBuffersAB;
        std::swap(swappedFrontierBuffers[7], swappedFrontierBuffers[12]);
        swappedFrontierBuffers[13] = m_buffers[14];
        updateDescriptorSet(m_device, descriptorSetBA, swappedFrontierBuffers);

        resetCommandPool();
        VkCommandBuffer commandBuffer = m_commandBuffer;
        const VkDescriptorSet finalFrontierDescriptorSet = recordPathLoopAndOptionalResolve(
          commandBuffer, m_clearPipeline, m_initializePipeline, m_prepareDispatchPipeline,
          m_advancePipeline, captureResolvedDisplay ? m_resolvePipeline : VK_NULL_HANDLE,
          m_pipelineLayout, descriptorSetAB, descriptorSetBA,
          static_cast<std::uint32_t>(std::max<std::size_t>(1u, launchPathCount)),
          m_buffers[13].buffer, m_buffers[14].buffer, plan.parameters.maxDepth,
          static_cast<std::uint32_t>(std::max<std::uint64_t>(1u, resolvedDisplayPixels)),
          captureResolvedDisplay);
        const std::size_t finalFrontierBufferIndex =
          finalFrontierDescriptorSet == descriptorSetAB ? 7u : 12u;
        const auto uploadEnd = std::chrono::steady_clock::now();

        const auto kernelStart = std::chrono::steady_clock::now();
        submitAndWait(m_queue, commandBuffer);
        const auto kernelEnd = std::chrono::steady_clock::now();

        const auto readbackStart = std::chrono::steady_clock::now();
        VulkanGpuDiffusePathLoopKernelResult result;
        result.executionPath = "vulkan_diffuse_path_loop_wavefront";
        result.pathStateResidency = "vulkan_host_visible_diffuse_path_state";
        result.retainedFrontierDispatchesIndirect = true;
        result.sceneUploadCacheHit = sceneUploadCacheHit;
        result.sceneUploadBytesWritten =
          sceneUploadCacheHit ? 0u : static_cast<std::uint64_t>(plan.sceneUpload.size());
        result.bufferSizes = plan.buffers;
        result.bufferSizes.totalResidentBytes -= plan.buffers.activePathStateBytes;
        result.bufferSizes.totalResidentBytes -= plan.buffers.nextPathStateBytes;
        result.bufferSizes.activePathStateBytes = pathStateStorageBytes;
        result.bufferSizes.nextPathStateBytes = 0u;
        result.bufferSizes.totalResidentBytes += pathStateStorageBytes;
        result.bufferSizes.totalResidentBytes += plan.buffers.retainedIndexBytes;
        result.uploadWorkerSeconds = secondsBetween(uploadStart, uploadEnd);
        result.kernelWorkerSeconds = secondsBetween(kernelStart, kernelEnd);
        result.echoedParameters = readBackOne<GpuDiffusePathLoopLaunchParameters>(
          m_device, m_buffers[1].memory, "Vulkan diffuse path-loop echoed parameters mapping");
        if (plan.parameters.captureMetrics != 0u || plan.parameters.captureDiagnostics != 0u) {
          const std::vector<std::uint32_t> retainedCountOutput = readBackRecords<std::uint32_t>(
            m_device, m_buffers[finalFrontierBufferIndex].memory, byteCount<std::uint32_t>(1u), 1u,
            "Vulkan diffuse path-loop retained-count output mapping");
          result.retainedPathCount =
            retainedPathCountFromBuffer(retainedCountOutput, launchPathCount);
        }
        if (plan.parameters.captureDiagnostics != 0u) {
          result.resolvedPathStates = readBackRecords<GpuDiffusePathStateRecord>(
            m_device, m_buffers[4].memory, byteCount<GpuDiffusePathStateRecord>(launchPathCount),
            launchPathCount, "Vulkan diffuse path-loop active path-state output mapping");
          result.nextPathStates = readBackRecords<GpuDiffusePathStateRecord>(
            m_device, m_buffers[4].memory, byteCount<GpuDiffusePathStateRecord>(launchPathCount),
            launchPathCount, "Vulkan diffuse path-loop final path-state output mapping");
          const std::size_t rawStepCount =
            launchPathCount * static_cast<std::size_t>(plan.parameters.maxDepth);
          const std::vector<GpuDiffusePathStepRecord> rawStepRecords =
            readBackRecords<GpuDiffusePathStepRecord>(
              m_device, m_buffers[6].memory, byteCount<GpuDiffusePathStepRecord>(rawStepCount),
              rawStepCount, "Vulkan diffuse path-loop step-record output mapping");
          result.stepRecords.reserve(rawStepRecords.size());
          for (const GpuDiffusePathStepRecord& step : rawStepRecords) {
            if (static_cast<GpuDiffusePathStepEvent>(step.event) !=
                GpuDiffusePathStepEvent::Inactive) {
              result.stepRecords.push_back(step);
            }
          }
          const std::vector<std::uint32_t> retainedOutput = readBackRecords<std::uint32_t>(
            m_device, m_buffers[finalFrontierBufferIndex].memory,
            byteCount<std::uint32_t>(retainedIndices.size()), retainedIndices.size(),
            "Vulkan diffuse path-loop retained-index output mapping");
          result.retainedPathIndices =
            retainedPathIndicesFromBuffer(retainedOutput, launchPathCount);
          result.retainedPathCount = static_cast<std::uint32_t>(result.retainedPathIndices.size());
        }
        if (captureResolvedDisplay) {
          result.resolvedDisplayPixels = readBackRecords<unsigned int>(
            m_device, m_buffers[11].memory, byteCount<unsigned int>(resolvedDisplayPixels),
            resolvedDisplayPixels, "Vulkan diffuse path-loop display resolve output mapping");
        }
        if (capturePlatformAccumulation) {
          result.accumulationColorSums = readBackRecords<std::array<float, 4>>(
            m_device, m_buffers[8].memory, byteCount<std::array<float, 4>>(pixels), pixels,
            "Vulkan diffuse path-loop accumulation color output mapping");
          const VkDeviceSize colorBytes = byteCount<std::array<float, 4>>(pixels);
          result.accumulationSampleCounts = readBackRecords<std::uint32_t>(
            m_device, m_buffers[8].memory, byteCount<std::uint32_t>(pixels), pixels, colorBytes,
            "Vulkan diffuse path-loop accumulation count output mapping");
        }
        if (plan.parameters.captureDenoiserFeatures != 0u) {
          result.denoiserFeatureRecords = readBackRecords<GpuDiffusePathDenoiserFeatureRecord>(
            m_device, m_buffers[9].memory, byteCount<GpuDiffusePathDenoiserFeatureRecord>(pixels),
            pixels, "Vulkan diffuse path-loop denoiser feature output mapping");
        }
        if (plan.parameters.captureMetrics != 0u) {
          result.activePathCountsPerDepth = readBackRecords<std::uint32_t>(
            m_device, m_buffers[10].memory, byteCount<std::uint32_t>(plan.parameters.maxDepth),
            plan.parameters.maxDepth, "Vulkan diffuse path-loop active-depth count mapping");
        }
        result.readbackWorkerSeconds =
          secondsBetween(readbackStart, std::chrono::steady_clock::now());
        return result;
      }

    private:
      struct DeviceSelection {
        VkPhysicalDevice device{VK_NULL_HANDLE};
        std::uint32_t queueFamily{kInvalidQueueFamily};
      };

      struct StorageBuffer {
        VkBuffer buffer{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkDeviceSize byteCount{0};
        VkDeviceSize capacityByteCount{0};
        VkBufferUsageFlags usage{0};
      };

      struct ShaderGuard {
        ~ShaderGuard() {
          if (shaderModule) {
            vkDestroyShaderModule(device, shaderModule, nullptr);
          }
        }

        VkDevice device{VK_NULL_HANDLE};
        VkShaderModule shaderModule{VK_NULL_HANDLE};
      };

      static constexpr std::uint32_t kInvalidQueueFamily =
        std::numeric_limits<std::uint32_t>::max();

      void initialize() {
        m_instance = createInstance();
        m_selection = selectDevice(m_instance);
        if (m_selection.device == VK_NULL_HANDLE) {
          throw std::runtime_error(
            "Vulkan diffuse path-loop found no physical device with a compute queue");
        }

        m_device = createDevice(m_selection.device, m_selection.queueFamily);
        vkGetDeviceQueue(m_device, m_selection.queueFamily, 0, &m_queue);
        m_commandPool = createCommandPool(m_device, m_selection.queueFamily);
        m_commandBuffer = allocateCommandBuffer(m_device, m_commandPool);

        VkShaderModule clearShader = createShaderModule(
          m_device, vulkan_shaders::diffusePathLoopClearFrontierShaderSpirv.data(),
          vulkan_shaders::diffusePathLoopClearFrontierShaderSpirv.size());
        ShaderGuard clearShaderGuard;
        clearShaderGuard.device = m_device;
        clearShaderGuard.shaderModule = clearShader;
        VkShaderModule initializeShader = createShaderModule(
          m_device, vulkan_shaders::diffusePathLoopInitializeFrontierShaderSpirv.data(),
          vulkan_shaders::diffusePathLoopInitializeFrontierShaderSpirv.size());
        ShaderGuard initializeShaderGuard;
        initializeShaderGuard.device = m_device;
        initializeShaderGuard.shaderModule = initializeShader;
        VkShaderModule prepareDispatchShader = createShaderModule(
          m_device, vulkan_shaders::diffusePathLoopPrepareDispatchShaderSpirv.data(),
          vulkan_shaders::diffusePathLoopPrepareDispatchShaderSpirv.size());
        ShaderGuard prepareDispatchShaderGuard;
        prepareDispatchShaderGuard.device = m_device;
        prepareDispatchShaderGuard.shaderModule = prepareDispatchShader;
        VkShaderModule advanceShader = createShaderModule(
          m_device, vulkan_shaders::diffusePathLoopAdvanceFrontierShaderSpirv.data(),
          vulkan_shaders::diffusePathLoopAdvanceFrontierShaderSpirv.size());
        ShaderGuard advanceShaderGuard;
        advanceShaderGuard.device = m_device;
        advanceShaderGuard.shaderModule = advanceShader;
        VkShaderModule resolveShader = createShaderModule(
          m_device, vulkan_shaders::diffusePathLoopDisplayResolveShaderSpirv.data(),
          vulkan_shaders::diffusePathLoopDisplayResolveShaderSpirv.size());
        ShaderGuard resolveShaderGuard;
        resolveShaderGuard.device = m_device;
        resolveShaderGuard.shaderModule = resolveShader;

        m_descriptorLayout = createDescriptorLayout(m_device, kDiffusePathLoopDescriptorCount);
        m_descriptorPool = createDescriptorPool(m_device, kDiffusePathLoopDescriptorCount, 2u);
        m_descriptorSetAB = allocateDescriptorSet(m_device, m_descriptorPool, m_descriptorLayout);
        m_descriptorSetBA = allocateDescriptorSet(m_device, m_descriptorPool, m_descriptorLayout);
        m_pipelineLayout = createPipelineLayout(m_device, m_descriptorLayout);
        m_clearPipeline = createPipeline(m_device, clearShader, m_pipelineLayout);
        m_initializePipeline = createPipeline(m_device, initializeShader, m_pipelineLayout);
        m_prepareDispatchPipeline =
          createPipeline(m_device, prepareDispatchShader, m_pipelineLayout);
        m_advancePipeline = createPipeline(m_device, advanceShader, m_pipelineLayout);
        m_resolvePipeline = createPipeline(m_device, resolveShader, m_pipelineLayout);

        clearShaderGuard.shaderModule = VK_NULL_HANDLE;
        initializeShaderGuard.shaderModule = VK_NULL_HANDLE;
        prepareDispatchShaderGuard.shaderModule = VK_NULL_HANDLE;
        advanceShaderGuard.shaderModule = VK_NULL_HANDLE;
        resolveShaderGuard.shaderModule = VK_NULL_HANDLE;
        vkDestroyShaderModule(m_device, clearShader, nullptr);
        vkDestroyShaderModule(m_device, initializeShader, nullptr);
        vkDestroyShaderModule(m_device, prepareDispatchShader, nullptr);
        vkDestroyShaderModule(m_device, advanceShader, nullptr);
        vkDestroyShaderModule(m_device, resolveShader, nullptr);
      }

      void destroy() {
        if (m_device) {
          vkDeviceWaitIdle(m_device);
          destroyStorageBuffers();
          if (m_commandPool) {
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
            m_commandBuffer = VK_NULL_HANDLE;
          }
          if (m_resolvePipeline) {
            vkDestroyPipeline(m_device, m_resolvePipeline, nullptr);
            m_resolvePipeline = VK_NULL_HANDLE;
          }
          if (m_advancePipeline) {
            vkDestroyPipeline(m_device, m_advancePipeline, nullptr);
            m_advancePipeline = VK_NULL_HANDLE;
          }
          if (m_prepareDispatchPipeline) {
            vkDestroyPipeline(m_device, m_prepareDispatchPipeline, nullptr);
            m_prepareDispatchPipeline = VK_NULL_HANDLE;
          }
          if (m_initializePipeline) {
            vkDestroyPipeline(m_device, m_initializePipeline, nullptr);
            m_initializePipeline = VK_NULL_HANDLE;
          }
          if (m_clearPipeline) {
            vkDestroyPipeline(m_device, m_clearPipeline, nullptr);
            m_clearPipeline = VK_NULL_HANDLE;
          }
          if (m_pipelineLayout) {
            vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
            m_pipelineLayout = VK_NULL_HANDLE;
          }
          if (m_descriptorPool) {
            vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
            m_descriptorSetAB = VK_NULL_HANDLE;
            m_descriptorSetBA = VK_NULL_HANDLE;
          }
          if (m_descriptorLayout) {
            vkDestroyDescriptorSetLayout(m_device, m_descriptorLayout, nullptr);
            m_descriptorLayout = VK_NULL_HANDLE;
          }
          vkDestroyDevice(m_device, nullptr);
          m_device = VK_NULL_HANDLE;
        }
        if (m_instance) {
          vkDestroyInstance(m_instance, nullptr);
          m_instance = VK_NULL_HANDLE;
        }
        m_queue = VK_NULL_HANDLE;
        m_selection = {};
      }

      void check(VkResult result, const char* operation) const {
        if (result != VK_SUCCESS) {
          throw std::runtime_error(std::string(operation) + " failed");
        }
      }

      VkInstance createInstance() const {
        VkApplicationInfo applicationInfo{};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "raytracer Vulkan diffuse path loop";
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pEngineName = "raytracer";
        applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &applicationInfo;

        VkInstance instance = VK_NULL_HANDLE;
        check(vkCreateInstance(&createInfo, nullptr, &instance),
              "Vulkan diffuse path-loop instance creation");
        return instance;
      }

      std::uint32_t computeQueueFamily(VkPhysicalDevice device) const {
        std::uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        if (queueFamilyCount == 0) {
          return kInvalidQueueFamily;
        }

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
        for (std::uint32_t index = 0; index != queueFamilyCount; ++index) {
          const VkQueueFamilyProperties& queueFamily = queueFamilies[index];
          if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 && queueFamily.queueCount > 0) {
            return index;
          }
        }
        return kInvalidQueueFamily;
      }

      DeviceSelection selectDevice(VkInstance instance) const {
        std::uint32_t deviceCount = 0;
        if (vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr) != VK_SUCCESS ||
            deviceCount == 0) {
          return {};
        }

        std::vector<VkPhysicalDevice> devices(deviceCount, VK_NULL_HANDLE);
        if (vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()) != VK_SUCCESS) {
          return {};
        }

        for (VkPhysicalDevice device : devices) {
          const std::uint32_t queueFamily = computeQueueFamily(device);
          if (queueFamily != kInvalidQueueFamily) {
            return {device, queueFamily};
          }
        }
        return {};
      }

      VkDevice createDevice(VkPhysicalDevice physicalDevice, std::uint32_t queueFamily) const {
        const float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

        VkDevice device = VK_NULL_HANDLE;
        check(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device),
              "Vulkan diffuse path-loop logical device creation");
        return device;
      }

      std::uint32_t findHostVisibleMemoryType(VkPhysicalDevice physicalDevice,
                                              std::uint32_t memoryTypeBits) const {
        VkPhysicalDeviceMemoryProperties properties{};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
        for (std::uint32_t index = 0; index != properties.memoryTypeCount; ++index) {
          const bool typeSupported = (memoryTypeBits & (1u << index)) != 0;
          const VkMemoryPropertyFlags flags = properties.memoryTypes[index].propertyFlags;
          if (typeSupported && (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 &&
              (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0) {
            return index;
          }
        }
        throw std::runtime_error("Vulkan diffuse path-loop requires host-coherent buffer memory");
      }

      StorageBuffer createStorageBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                                        VkDeviceSize byteCount, const void* initialData,
                                        VkBufferUsageFlags additionalUsage = 0) const {
        StorageBuffer result;
        result.byteCount = std::max<VkDeviceSize>(1u, byteCount);
        result.capacityByteCount = result.byteCount;
        result.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | additionalUsage;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = result.capacityByteCount;
        bufferInfo.usage = result.usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device, &bufferInfo, nullptr, &result.buffer),
              "Vulkan diffuse path-loop buffer creation");

        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, result.buffer, &requirements);

        VkMemoryAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = requirements.size;
        allocateInfo.memoryTypeIndex =
          findHostVisibleMemoryType(physicalDevice, requirements.memoryTypeBits);
        check(vkAllocateMemory(device, &allocateInfo, nullptr, &result.memory),
              "Vulkan diffuse path-loop buffer memory allocation");
        check(vkBindBufferMemory(device, result.buffer, result.memory, 0),
              "Vulkan diffuse path-loop buffer binding");

        if (initialData && byteCount != 0u) {
          writeBuffer(device, result, byteCount, initialData,
                      "Vulkan diffuse path-loop input buffer mapping");
        }
        return result;
      }

      void destroyStorageBuffer(StorageBuffer& buffer) const {
        if (buffer.buffer) {
          vkDestroyBuffer(m_device, buffer.buffer, nullptr);
        }
        if (buffer.memory) {
          vkFreeMemory(m_device, buffer.memory, nullptr);
        }
        buffer = {};
      }

      void destroyStorageBuffers() const {
        for (StorageBuffer& buffer : m_buffers) {
          destroyStorageBuffer(buffer);
        }
        m_buffers.clear();
        m_sceneUploadCache.clear();
      }

      StorageBuffer& prepareStorageBuffer(std::size_t index, VkDeviceSize byteCount,
                                          const void* initialData,
                                          VkBufferUsageFlags additionalUsage = 0) const {
        if (m_buffers.size() < kDiffusePathLoopDescriptorCount + 1u) {
          m_buffers.resize(kDiffusePathLoopDescriptorCount + 1u);
        }

        const VkDeviceSize logicalByteCount = std::max<VkDeviceSize>(1u, byteCount);
        const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | additionalUsage;
        StorageBuffer& buffer = m_buffers[index];
        if (buffer.buffer == VK_NULL_HANDLE || buffer.capacityByteCount < logicalByteCount ||
            buffer.usage != usage) {
          destroyStorageBuffer(buffer);
          buffer = createStorageBuffer(m_device, m_selection.device, logicalByteCount, nullptr,
                                       additionalUsage);
        }
        buffer.byteCount = logicalByteCount;
        if (initialData && byteCount != 0u) {
          writeBuffer(m_device, buffer, byteCount, initialData,
                      "Vulkan diffuse path-loop input buffer mapping");
        }
        return buffer;
      }

      void writeBuffer(VkDevice device, const StorageBuffer& buffer, VkDeviceSize byteCount,
                       const void* data, const char* operation) const {
        void* mapped = nullptr;
        check(vkMapMemory(device, buffer.memory, 0, byteCount, 0, &mapped), operation);
        std::memcpy(mapped, data, static_cast<std::size_t>(byteCount));
        vkUnmapMemory(device, buffer.memory);
      }

      template<typename Record>
      static VkDeviceSize byteCount(std::size_t recordCount) {
        if (recordCount >
            std::numeric_limits<VkDeviceSize>::max() / static_cast<VkDeviceSize>(sizeof(Record))) {
          throw std::runtime_error("Vulkan diffuse path-loop buffer is too large");
        }
        return static_cast<VkDeviceSize>(recordCount) * static_cast<VkDeviceSize>(sizeof(Record));
      }

      template<typename Record>
      StorageBuffer& prepareStorageBufferFromVector(std::size_t index,
                                                    const std::vector<Record>& records,
                                                    VkBufferUsageFlags additionalUsage = 0) const {
        return prepareStorageBuffer(index, byteCount<Record>(records.size()),
                                    records.empty() ? nullptr : records.data(), additionalUsage);
      }

      StorageBuffer& prepareStorageBufferFromBytes(std::size_t index,
                                                   const std::vector<std::uint8_t>& bytes,
                                                   VkBufferUsageFlags additionalUsage = 0) const {
        return prepareStorageBuffer(index, static_cast<VkDeviceSize>(bytes.size()),
                                    bytes.empty() ? nullptr : bytes.data(), additionalUsage);
      }

      bool prepareStorageBufferFromBytesIfChanged(std::size_t index,
                                                  const std::vector<std::uint8_t>& bytes,
                                                  std::vector<std::uint8_t>& cachedBytes,
                                                  VkBufferUsageFlags additionalUsage = 0) const {
        const VkDeviceSize logicalByteCount =
          std::max<VkDeviceSize>(1u, static_cast<VkDeviceSize>(bytes.size()));
        const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | additionalUsage;
        const bool bufferReusable =
          index < m_buffers.size() && m_buffers[index].buffer != VK_NULL_HANDLE &&
          m_buffers[index].capacityByteCount >= logicalByteCount && m_buffers[index].usage == usage;
        const bool bytesUnchanged = bufferReusable && cachedBytes == bytes;
        prepareStorageBuffer(index, static_cast<VkDeviceSize>(bytes.size()),
                             bytesUnchanged || bytes.empty() ? nullptr : bytes.data(),
                             additionalUsage);
        if (!bytesUnchanged) {
          cachedBytes = bytes;
        }
        return bytesUnchanged;
      }

      VkShaderModule createShaderModule(VkDevice device, const std::uint32_t* words,
                                        std::size_t wordCount) const {
        VkShaderModuleCreateInfo shaderInfo{};
        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = wordCount * sizeof(std::uint32_t);
        shaderInfo.pCode = words;

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        check(vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule),
              "Vulkan diffuse path-loop shader module creation");
        return shaderModule;
      }

      VkDescriptorSetLayout createDescriptorLayout(VkDevice device,
                                                   std::uint32_t bindingCount) const {
        std::vector<VkDescriptorSetLayoutBinding> bindings(bindingCount);
        for (std::uint32_t index = 0; index != bindingCount; ++index) {
          bindings[index].binding = index;
          bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
          bindings[index].descriptorCount = 1;
          bindings[index].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
        descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorLayoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
        descriptorLayoutInfo.pBindings = bindings.data();

        VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
        check(
          vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr, &descriptorLayout),
          "Vulkan diffuse path-loop descriptor layout creation");
        return descriptorLayout;
      }

      VkPipelineLayout createPipelineLayout(VkDevice device,
                                            VkDescriptorSetLayout descriptorLayout) const {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorLayout;

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        check(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout),
              "Vulkan diffuse path-loop pipeline layout creation");
        return pipelineLayout;
      }

      VkPipeline createPipeline(VkDevice device, VkShaderModule shaderModule,
                                VkPipelineLayout pipelineLayout) const {
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = shaderModule;
        pipelineInfo.stage.pName = "main";
        pipelineInfo.layout = pipelineLayout;

        VkPipeline pipeline = VK_NULL_HANDLE;
        check(
          vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline),
          "Vulkan diffuse path-loop compute pipeline creation");
        return pipeline;
      }

      VkDescriptorPool createDescriptorPool(VkDevice device, std::uint32_t descriptorCount,
                                            std::uint32_t setCount = 1u) const {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = descriptorCount * setCount;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = setCount;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        check(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool),
              "Vulkan diffuse path-loop descriptor pool creation");
        return descriptorPool;
      }

      VkDescriptorSet allocateDescriptorSet(VkDevice device, VkDescriptorPool descriptorPool,
                                            VkDescriptorSetLayout descriptorLayout) const {
        VkDescriptorSetAllocateInfo descriptorAllocateInfo{};
        descriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorAllocateInfo.descriptorPool = descriptorPool;
        descriptorAllocateInfo.descriptorSetCount = 1;
        descriptorAllocateInfo.pSetLayouts = &descriptorLayout;

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        check(vkAllocateDescriptorSets(device, &descriptorAllocateInfo, &descriptorSet),
              "Vulkan diffuse path-loop descriptor set allocation");
        return descriptorSet;
      }

      void updateDescriptorSet(VkDevice device, VkDescriptorSet descriptorSet,
                               const std::vector<StorageBuffer>& buffers) const {
        std::vector<VkDescriptorBufferInfo> descriptors(buffers.size());
        std::vector<VkWriteDescriptorSet> descriptorWrites(buffers.size());
        for (std::size_t index = 0; index != buffers.size(); ++index) {
          descriptors[index].buffer = buffers[index].buffer;
          descriptors[index].offset = 0;
          descriptors[index].range = buffers[index].byteCount;

          descriptorWrites[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          descriptorWrites[index].dstSet = descriptorSet;
          descriptorWrites[index].dstBinding = static_cast<std::uint32_t>(index);
          descriptorWrites[index].descriptorCount = 1;
          descriptorWrites[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
          descriptorWrites[index].pBufferInfo = &descriptors[index];
        }
        vkUpdateDescriptorSets(device, static_cast<std::uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
      }

      VkCommandPool createCommandPool(VkDevice device, std::uint32_t queueFamily) const {
        VkCommandPoolCreateInfo commandPoolInfo{};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.queueFamilyIndex = queueFamily;

        VkCommandPool commandPool = VK_NULL_HANDLE;
        check(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool),
              "Vulkan diffuse path-loop command pool creation");
        return commandPool;
      }

      VkCommandBuffer allocateCommandBuffer(VkDevice device, VkCommandPool commandPool) const {
        VkCommandBufferAllocateInfo commandAllocateInfo{};
        commandAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandAllocateInfo.commandPool = commandPool;
        commandAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandAllocateInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        check(vkAllocateCommandBuffers(device, &commandAllocateInfo, &commandBuffer),
              "Vulkan diffuse path-loop command buffer allocation");
        return commandBuffer;
      }

      void resetCommandPool() const {
        check(vkResetCommandPool(m_device, m_commandPool, 0),
              "Vulkan diffuse path-loop command pool reset");
      }

      void shaderStorageBarrier(VkCommandBuffer commandBuffer) const {
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0,
                             nullptr);
      }

      void shaderToIndirectDispatchBarrier(VkCommandBuffer commandBuffer) const {
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 1, &barrier, 0, nullptr, 0,
                             nullptr);
      }

      void dispatch1D(VkCommandBuffer commandBuffer, std::uint32_t itemCount) const {
        const std::uint32_t groupCount =
          std::max(1u, (itemCount + kDiffusePathLoopLocalSizeX - 1u) / kDiffusePathLoopLocalSizeX);
        vkCmdDispatch(commandBuffer, groupCount, 1, 1);
      }

      VkDescriptorSet recordPathLoopAndOptionalResolve(
        VkCommandBuffer commandBuffer, VkPipeline clearFrontierPipeline,
        VkPipeline initializeFrontierPipeline, VkPipeline prepareDispatchPipeline,
        VkPipeline advanceFrontierPipeline, VkPipeline resolvePipeline,
        VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSetAB,
        VkDescriptorSet descriptorSetBA, std::uint32_t pathCount, VkBuffer dispatchBufferA,
        VkBuffer dispatchBufferB, std::uint32_t maxDepth, std::uint32_t resolvedDisplayPixels,
        bool captureResolvedDisplay) const {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        check(vkBeginCommandBuffer(commandBuffer, &beginInfo),
              "Vulkan diffuse path-loop command buffer begin");

        VkDescriptorSet currentDescriptorSet = descriptorSetAB;
        VkDescriptorSet nextDescriptorSet = descriptorSetBA;
        VkBuffer currentDispatchBuffer = dispatchBufferA;
        VkBuffer nextDispatchBuffer = dispatchBufferB;

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, clearFrontierPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1,
                                &currentDescriptorSet, 0, nullptr);
        vkCmdDispatch(commandBuffer, 1, 1, 1);
        shaderStorageBarrier(commandBuffer);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1,
                                &nextDescriptorSet, 0, nullptr);
        vkCmdDispatch(commandBuffer, 1, 1, 1);
        shaderStorageBarrier(commandBuffer);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          initializeFrontierPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1,
                                &currentDescriptorSet, 0, nullptr);
        dispatch1D(commandBuffer, pathCount);
        shaderStorageBarrier(commandBuffer);

        for (std::uint32_t depth = 0; depth != maxDepth; ++depth) {
          (void)depth;
          vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, clearFrontierPipeline);
          vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0,
                                  1, &nextDescriptorSet, 0, nullptr);
          vkCmdDispatch(commandBuffer, 1, 1, 1);
          shaderStorageBarrier(commandBuffer);

          vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, prepareDispatchPipeline);
          vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0,
                                  1, &currentDescriptorSet, 0, nullptr);
          vkCmdDispatch(commandBuffer, 1, 1, 1);
          shaderToIndirectDispatchBarrier(commandBuffer);

          vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, advanceFrontierPipeline);
          vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0,
                                  1, &currentDescriptorSet, 0, nullptr);
          vkCmdDispatchIndirect(commandBuffer, currentDispatchBuffer, 0);
          shaderStorageBarrier(commandBuffer);
          std::swap(currentDescriptorSet, nextDescriptorSet);
          std::swap(currentDispatchBuffer, nextDispatchBuffer);
        }

        if (captureResolvedDisplay) {
          vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, resolvePipeline);
          vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0,
                                  1, &currentDescriptorSet, 0, nullptr);
          dispatch1D(commandBuffer, resolvedDisplayPixels);
        }
        check(vkEndCommandBuffer(commandBuffer), "Vulkan diffuse path-loop command buffer end");
        return currentDescriptorSet;
      }

      void submitAndWait(VkQueue queue, VkCommandBuffer commandBuffer) const {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        check(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE),
              "Vulkan diffuse path-loop queue submit");
        check(vkQueueWaitIdle(queue), "Vulkan diffuse path-loop queue wait");
      }

      template<typename Record>
      Record readBackOne(VkDevice device, VkDeviceMemory outputMemory,
                         const char* operation) const {
        return readBackRecords<Record>(device, outputMemory, sizeof(Record), 1, operation).front();
      }

      template<typename Record>
      std::vector<Record> readBackRecords(VkDevice device, VkDeviceMemory outputMemory,
                                          VkDeviceSize byteCount, std::size_t resultCount,
                                          const char* operation) const {
        return readBackRecords<Record>(device, outputMemory, byteCount, resultCount, 0u, operation);
      }

      template<typename Record>
      std::vector<Record> readBackRecords(VkDevice device, VkDeviceMemory outputMemory,
                                          VkDeviceSize byteCount, std::size_t resultCount,
                                          VkDeviceSize byteOffset, const char* operation) const {
        std::vector<Record> results(resultCount);
        if (resultCount == 0u) {
          return results;
        }
        void* mapped = nullptr;
        check(vkMapMemory(device, outputMemory, byteOffset, byteCount, 0, &mapped), operation);
        std::memcpy(results.data(), mapped, static_cast<std::size_t>(byteCount));
        vkUnmapMemory(device, outputMemory);
        return results;
      }

      mutable std::mutex m_runMutex;
      std::string m_unavailableReason;
      VkInstance m_instance{VK_NULL_HANDLE};
      DeviceSelection m_selection;
      VkDevice m_device{VK_NULL_HANDLE};
      VkQueue m_queue{VK_NULL_HANDLE};
      VkDescriptorSetLayout m_descriptorLayout{VK_NULL_HANDLE};
      VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
      VkDescriptorSet m_descriptorSetAB{VK_NULL_HANDLE};
      VkDescriptorSet m_descriptorSetBA{VK_NULL_HANDLE};
      VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
      VkPipeline m_clearPipeline{VK_NULL_HANDLE};
      VkPipeline m_initializePipeline{VK_NULL_HANDLE};
      VkPipeline m_prepareDispatchPipeline{VK_NULL_HANDLE};
      VkPipeline m_advancePipeline{VK_NULL_HANDLE};
      VkPipeline m_resolvePipeline{VK_NULL_HANDLE};
      VkCommandPool m_commandPool{VK_NULL_HANDLE};
      VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE};
      mutable std::vector<StorageBuffer> m_buffers;
      mutable std::vector<std::uint8_t> m_sceneUploadCache;
    };

    const VulkanDiffusePathLoopRuntime& sharedVulkanDiffusePathLoopRuntime() {
      static const VulkanDiffusePathLoopRuntime runtime;
      return runtime;
    }
#endif
  }

  bool VulkanGpuDiffusePathLoopKernel::deviceAvailable() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return sharedVulkanDiffusePathLoopRuntime().deviceAvailable();
#else
    return false;
#endif
  }

  std::string VulkanGpuDiffusePathLoopKernel::deviceUnavailableReason() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return sharedVulkanDiffusePathLoopRuntime().deviceUnavailableReason();
#else
    return "Vulkan diffuse path-loop backend is not enabled in this build";
#endif
  }

  bool VulkanGpuDiffusePathLoopKernel::launchPathAvailable() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return sharedVulkanDiffusePathLoopRuntime().launchPathAvailable();
#else
    return false;
#endif
  }

  std::string VulkanGpuDiffusePathLoopKernel::launchPathUnavailableReason() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return sharedVulkanDiffusePathLoopRuntime().launchPathUnavailableReason();
#else
    return "Vulkan diffuse path-loop backend is not enabled in this build";
#endif
  }

  VulkanGpuDiffusePathLoopKernelResult VulkanGpuDiffusePathLoopKernel::runWavefrontPathLoop(
    const GpuDiffusePathLoopLaunchPlan& plan,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
    bool capturePlatformAccumulation, bool captureResolvedDisplay) const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return sharedVulkanDiffusePathLoopRuntime().runWavefrontPathLoop(
      plan, initialPathStates, capturePlatformAccumulation, captureResolvedDisplay);
#else
    (void)plan;
    (void)initialPathStates;
    (void)capturePlatformAccumulation;
    (void)captureResolvedDisplay;
    throw std::runtime_error(launchPathUnavailableReason());
#endif
  }
}
