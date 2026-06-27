#include "render/VulkanGpuDiffusePathLoopKernel.h"

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
#include "render/VulkanDiffusePathLoopAllMiss.generated.h"
#include "render/VulkanDiffusePathLoopDisplayResolve.generated.h"

#include <vulkan/vulkan.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
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
    static_assert(sizeof(GpuDiffusePathStepRecord) == 96);
    static_assert(alignof(GpuDiffusePathStepRecord) == 16);
    static_assert(sizeof(GpuDiffusePathDenoiserFeatureRecord) == 64);
    static_assert(alignof(GpuDiffusePathDenoiserFeatureRecord) == 16);
    static_assert(sizeof(GpuTracingEnvironmentRecord) == 32);
    static_assert(alignof(GpuTracingEnvironmentRecord) == 16);

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    constexpr std::uint32_t kDiffusePathLoopLocalSizeX = 64u;
    constexpr std::uint32_t kDiffusePathLoopDescriptorCount = 12u;

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
      bool deviceAvailable() const {
        return deviceUnavailableReason().empty();
      }

      std::string deviceUnavailableReason() const {
        VkInstance instance = VK_NULL_HANDLE;
        try {
          instance = createInstance();
        } catch (const std::runtime_error& e) {
          return e.what();
        }

        InstanceGuard instanceGuard;
        instanceGuard.instance = instance;
        const DeviceSelection selection = selectDevice(instance);
        if (selection.device == VK_NULL_HANDLE) {
          return "Vulkan diffuse path-loop found no physical device with a compute queue";
        }
        return "";
      }

      bool launchPathAvailable() const {
        return launchPathUnavailableReason().empty();
      }

      std::string launchPathUnavailableReason() const {
        try {
          VkInstance instance = createInstance();
          InstanceGuard instanceGuard;
          instanceGuard.instance = instance;

          const DeviceSelection selection = selectDevice(instance);
          if (selection.device == VK_NULL_HANDLE) {
            return "Vulkan diffuse path-loop found no physical device with a compute queue";
          }

          VkDevice device = createDevice(selection.device, selection.queueFamily);
          DeviceGuard deviceGuard;
          deviceGuard.device = device;

          VkShaderModule shader =
            createShaderModule(device, vulkan_shaders::diffusePathLoopAllMissShaderSpirv.data(),
                               vulkan_shaders::diffusePathLoopAllMissShaderSpirv.size());
          ShaderGuard shaderGuard;
          shaderGuard.device = device;
          shaderGuard.shaderModule = shader;
          VkShaderModule resolveShader = createShaderModule(
            device, vulkan_shaders::diffusePathLoopDisplayResolveShaderSpirv.data(),
            vulkan_shaders::diffusePathLoopDisplayResolveShaderSpirv.size());
          ShaderGuard resolveShaderGuard;
          resolveShaderGuard.device = device;
          resolveShaderGuard.shaderModule = resolveShader;

          VkDescriptorSetLayout descriptorLayout =
            createDescriptorLayout(device, kDiffusePathLoopDescriptorCount);
          DescriptorLayoutGuard descriptorLayoutGuard;
          descriptorLayoutGuard.device = device;
          descriptorLayoutGuard.layout = descriptorLayout;

          VkPipelineLayout pipelineLayout = createPipelineLayout(device, descriptorLayout);
          PipelineLayoutGuard pipelineLayoutGuard;
          pipelineLayoutGuard.device = device;
          pipelineLayoutGuard.layout = pipelineLayout;

          VkPipeline pipeline = createPipeline(device, shader, pipelineLayout);
          PipelineGuard pipelineGuard;
          pipelineGuard.device = device;
          pipelineGuard.pipeline = pipeline;
          VkPipeline resolvePipeline = createPipeline(device, resolveShader, pipelineLayout);
          PipelineGuard resolvePipelineGuard;
          resolvePipelineGuard.device = device;
          resolvePipelineGuard.pipeline = resolvePipeline;
          return "";
        } catch (const std::runtime_error& e) {
          return e.what();
        }
      }

      VulkanGpuDiffusePathLoopKernelResult
      runAllMissPathLoop(const GpuDiffusePathLoopLaunchPlan& plan,
                         const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
                         bool capturePlatformAccumulation, bool captureResolvedDisplay) const {
        validatePathLoopPlan(plan, initialPathStates);
        const std::size_t launchPathCount =
          static_cast<std::size_t>(plan.parameters.initialPathCount);

        const auto uploadStart = std::chrono::steady_clock::now();
        VkInstance instance = createInstance();
        InstanceGuard instanceGuard;
        instanceGuard.instance = instance;

        const DeviceSelection selection = selectDevice(instance);
        if (selection.device == VK_NULL_HANDLE) {
          throw std::runtime_error("Vulkan diffuse path-loop requires a compute device");
        }

        VkDevice device = createDevice(selection.device, selection.queueFamily);
        DeviceGuard deviceGuard;
        deviceGuard.device = device;

        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, selection.queueFamily, 0, &queue);

        const std::uint64_t pixels = pixelCount(plan.parameters);
        const std::uint64_t resolvedDisplayPixels =
          captureResolvedDisplay ? displayPixelCount(plan.parameters) : 0u;
        if (captureResolvedDisplay && resolvedDisplayPixels == 0u) {
          throw std::invalid_argument(
            "Vulkan diffuse path-loop display resolve requires pixel or sample-slot accumulation");
        }
        BufferVectorGuard buffers;
        buffers.device = device;
        buffers.buffers.push_back(createStorageBuffer(
          device, selection.device, sizeof(GpuDiffusePathLoopLaunchParameters), &plan.parameters));
        buffers.buffers.push_back(createStorageBuffer(
          device, selection.device, sizeof(GpuDiffusePathLoopLaunchParameters), nullptr));
        buffers.buffers.push_back(
          createStorageBufferFromBytes(device, selection.device, plan.sceneUpload));
        buffers.buffers.push_back(
          plan.generatesPrimaryPathsOnDevice()
            ? createStorageBuffer(device, selection.device, 1u, nullptr)
            : createStorageBufferFromVector(device, selection.device, initialPathStates));
        buffers.buffers.push_back(createStorageBuffer(
          device, selection.device, static_cast<VkDeviceSize>(plan.buffers.activePathStateBytes),
          nullptr));
        buffers.buffers.push_back(
          createStorageBuffer(device, selection.device,
                              static_cast<VkDeviceSize>(plan.buffers.nextPathStateBytes), nullptr));
        std::vector<std::uint8_t> stepRecordBytes(
          static_cast<std::size_t>(plan.buffers.stepRecordBytes), 0u);
        buffers.buffers.push_back(
          createStorageBufferFromBytes(device, selection.device, stepRecordBytes));
        const std::size_t retainedIndexCount =
          static_cast<std::size_t>(plan.buffers.retainedIndexBytes / sizeof(std::uint32_t));
        std::vector<std::uint32_t> retainedIndices(retainedIndexCount, 0u);
        buffers.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, retainedIndices));
        std::vector<std::uint8_t> accumulationBytes(
          static_cast<std::size_t>(std::max<std::uint64_t>(1u, plan.buffers.accumulationBytes)),
          0u);
        buffers.buffers.push_back(
          createStorageBufferFromBytes(device, selection.device, accumulationBytes));
        std::vector<std::uint8_t> denoiserFeatureBytes(
          static_cast<std::size_t>(
            std::max<std::uint64_t>(1u, plan.buffers.denoiserFeatureRecordBytes)),
          0u);
        buffers.buffers.push_back(
          createStorageBufferFromBytes(device, selection.device, denoiserFeatureBytes));
        std::vector<std::uint8_t> activePathCountBytes(
          static_cast<std::size_t>(std::max<std::uint64_t>(1u, plan.buffers.activePathCountBytes)),
          0u);
        buffers.buffers.push_back(
          createStorageBufferFromBytes(device, selection.device, activePathCountBytes));
        buffers.buffers.push_back(createStorageBuffer(
          device, selection.device,
          resolvedDisplayPixels * static_cast<std::uint64_t>(sizeof(unsigned int)), nullptr));

        VkShaderModule shader =
          createShaderModule(device, vulkan_shaders::diffusePathLoopAllMissShaderSpirv.data(),
                             vulkan_shaders::diffusePathLoopAllMissShaderSpirv.size());
        ShaderGuard shaderGuard;
        shaderGuard.device = device;
        shaderGuard.shaderModule = shader;
        VkShaderModule resolveShader =
          captureResolvedDisplay
            ? createShaderModule(device,
                                 vulkan_shaders::diffusePathLoopDisplayResolveShaderSpirv.data(),
                                 vulkan_shaders::diffusePathLoopDisplayResolveShaderSpirv.size())
            : VK_NULL_HANDLE;
        ShaderGuard resolveShaderGuard;
        resolveShaderGuard.device = device;
        resolveShaderGuard.shaderModule = resolveShader;

        VkDescriptorSetLayout descriptorLayout =
          createDescriptorLayout(device, kDiffusePathLoopDescriptorCount);
        DescriptorLayoutGuard descriptorLayoutGuard;
        descriptorLayoutGuard.device = device;
        descriptorLayoutGuard.layout = descriptorLayout;

        VkPipelineLayout pipelineLayout = createPipelineLayout(device, descriptorLayout);
        PipelineLayoutGuard pipelineLayoutGuard;
        pipelineLayoutGuard.device = device;
        pipelineLayoutGuard.layout = pipelineLayout;

        VkPipeline pipeline = createPipeline(device, shader, pipelineLayout);
        PipelineGuard pipelineGuard;
        pipelineGuard.device = device;
        pipelineGuard.pipeline = pipeline;
        VkPipeline resolvePipeline = captureResolvedDisplay
                                       ? createPipeline(device, resolveShader, pipelineLayout)
                                       : VK_NULL_HANDLE;
        PipelineGuard resolvePipelineGuard;
        resolvePipelineGuard.device = device;
        resolvePipelineGuard.pipeline = resolvePipeline;

        VkDescriptorPool descriptorPool =
          createDescriptorPool(device, kDiffusePathLoopDescriptorCount);
        DescriptorPoolGuard descriptorPoolGuard;
        descriptorPoolGuard.device = device;
        descriptorPoolGuard.pool = descriptorPool;

        VkDescriptorSet descriptorSet =
          allocateDescriptorSet(device, descriptorPool, descriptorLayout);
        updateDescriptorSet(device, descriptorSet, buffers.buffers);

        VkCommandPool commandPool = createCommandPool(device, selection.queueFamily);
        CommandPoolGuard commandPoolGuard;
        commandPoolGuard.device = device;
        commandPoolGuard.pool = commandPool;

        VkCommandBuffer commandBuffer = allocateCommandBuffer(device, commandPool);
        recordPathLoopAndOptionalResolve(
          commandBuffer, pipeline, resolvePipeline, pipelineLayout, descriptorSet,
          static_cast<std::uint32_t>(std::max<std::size_t>(1u, launchPathCount)),
          static_cast<std::uint32_t>(std::max<std::uint64_t>(1u, resolvedDisplayPixels)),
          captureResolvedDisplay);
        const auto uploadEnd = std::chrono::steady_clock::now();

        const auto kernelStart = std::chrono::steady_clock::now();
        submitAndWait(queue, commandBuffer);
        const auto kernelEnd = std::chrono::steady_clock::now();

        const auto readbackStart = std::chrono::steady_clock::now();
        VulkanGpuDiffusePathLoopKernelResult result;
        result.executionPath = "vulkan_diffuse_path_loop";
        result.pathStateResidency = "vulkan_host_visible_diffuse_path_state";
        result.bufferSizes = plan.buffers;
        result.uploadWorkerSeconds = secondsBetween(uploadStart, uploadEnd);
        result.kernelWorkerSeconds = secondsBetween(kernelStart, kernelEnd);
        result.echoedParameters = readBackOne<GpuDiffusePathLoopLaunchParameters>(
          device, buffers.buffers[1].memory, "Vulkan diffuse path-loop echoed parameters mapping");
        const std::vector<std::uint32_t> retainedCountOutput = readBackRecords<std::uint32_t>(
          device, buffers.buffers[7].memory, byteCount<std::uint32_t>(1u), 1u,
          "Vulkan diffuse path-loop retained-count output mapping");
        result.retainedPathCount =
          retainedPathCountFromBuffer(retainedCountOutput, launchPathCount);
        if (plan.parameters.captureDiagnostics != 0u) {
          result.resolvedPathStates = readBackRecords<GpuDiffusePathStateRecord>(
            device, buffers.buffers[4].memory,
            byteCount<GpuDiffusePathStateRecord>(launchPathCount), launchPathCount,
            "Vulkan diffuse path-loop active path-state output mapping");
          result.nextPathStates = readBackRecords<GpuDiffusePathStateRecord>(
            device, buffers.buffers[5].memory,
            byteCount<GpuDiffusePathStateRecord>(launchPathCount), launchPathCount,
            "Vulkan diffuse path-loop next path-state output mapping");
          const std::size_t rawStepCount =
            launchPathCount * static_cast<std::size_t>(plan.parameters.maxDepth);
          const std::vector<GpuDiffusePathStepRecord> rawStepRecords =
            readBackRecords<GpuDiffusePathStepRecord>(
              device, buffers.buffers[6].memory, byteCount<GpuDiffusePathStepRecord>(rawStepCount),
              rawStepCount, "Vulkan diffuse path-loop step-record output mapping");
          result.stepRecords.reserve(rawStepRecords.size());
          for (const GpuDiffusePathStepRecord& step : rawStepRecords) {
            if (static_cast<GpuDiffusePathStepEvent>(step.event) !=
                GpuDiffusePathStepEvent::Inactive) {
              result.stepRecords.push_back(step);
            }
          }
          const std::vector<std::uint32_t> retainedOutput = readBackRecords<std::uint32_t>(
            device, buffers.buffers[7].memory, byteCount<std::uint32_t>(retainedIndices.size()),
            retainedIndices.size(), "Vulkan diffuse path-loop retained-index output mapping");
          result.retainedPathIndices =
            retainedPathIndicesFromBuffer(retainedOutput, launchPathCount);
          result.retainedPathCount = static_cast<std::uint32_t>(result.retainedPathIndices.size());
        }
        if (captureResolvedDisplay) {
          result.resolvedDisplayPixels = readBackRecords<unsigned int>(
            device, buffers.buffers[11].memory, byteCount<unsigned int>(resolvedDisplayPixels),
            resolvedDisplayPixels, "Vulkan diffuse path-loop display resolve output mapping");
        }
        if (capturePlatformAccumulation) {
          result.accumulationColorSums = readBackRecords<std::array<float, 4>>(
            device, buffers.buffers[8].memory, byteCount<std::array<float, 4>>(pixels), pixels,
            "Vulkan diffuse path-loop accumulation color output mapping");
          const VkDeviceSize colorBytes = byteCount<std::array<float, 4>>(pixels);
          result.accumulationSampleCounts = readBackRecords<std::uint32_t>(
            device, buffers.buffers[8].memory, byteCount<std::uint32_t>(pixels), pixels, colorBytes,
            "Vulkan diffuse path-loop accumulation count output mapping");
        }
        if (plan.parameters.captureDenoiserFeatures != 0u) {
          result.denoiserFeatureRecords = readBackRecords<GpuDiffusePathDenoiserFeatureRecord>(
            device, buffers.buffers[9].memory,
            byteCount<GpuDiffusePathDenoiserFeatureRecord>(pixels), pixels,
            "Vulkan diffuse path-loop denoiser feature output mapping");
        }
        result.activePathCountsPerDepth = readBackRecords<std::uint32_t>(
          device, buffers.buffers[10].memory, byteCount<std::uint32_t>(plan.parameters.maxDepth),
          plan.parameters.maxDepth, "Vulkan diffuse path-loop active-depth count mapping");
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
      };

      struct InstanceGuard {
        ~InstanceGuard() {
          if (instance) {
            vkDestroyInstance(instance, nullptr);
          }
        }

        VkInstance instance{VK_NULL_HANDLE};
      };

      struct DeviceGuard {
        ~DeviceGuard() {
          if (device) {
            vkDestroyDevice(device, nullptr);
          }
        }

        VkDevice device{VK_NULL_HANDLE};
      };

      struct BufferVectorGuard {
        ~BufferVectorGuard() {
          for (StorageBuffer& buffer : buffers) {
            if (buffer.buffer) {
              vkDestroyBuffer(device, buffer.buffer, nullptr);
            }
            if (buffer.memory) {
              vkFreeMemory(device, buffer.memory, nullptr);
            }
          }
        }

        VkDevice device{VK_NULL_HANDLE};
        std::vector<StorageBuffer> buffers;
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

      struct DescriptorLayoutGuard {
        ~DescriptorLayoutGuard() {
          if (layout) {
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
          }
        }

        VkDevice device{VK_NULL_HANDLE};
        VkDescriptorSetLayout layout{VK_NULL_HANDLE};
      };

      struct PipelineLayoutGuard {
        ~PipelineLayoutGuard() {
          if (layout) {
            vkDestroyPipelineLayout(device, layout, nullptr);
          }
        }

        VkDevice device{VK_NULL_HANDLE};
        VkPipelineLayout layout{VK_NULL_HANDLE};
      };

      struct PipelineGuard {
        ~PipelineGuard() {
          if (pipeline) {
            vkDestroyPipeline(device, pipeline, nullptr);
          }
        }

        VkDevice device{VK_NULL_HANDLE};
        VkPipeline pipeline{VK_NULL_HANDLE};
      };

      struct DescriptorPoolGuard {
        ~DescriptorPoolGuard() {
          if (pool) {
            vkDestroyDescriptorPool(device, pool, nullptr);
          }
        }

        VkDevice device{VK_NULL_HANDLE};
        VkDescriptorPool pool{VK_NULL_HANDLE};
      };

      struct CommandPoolGuard {
        ~CommandPoolGuard() {
          if (pool) {
            vkDestroyCommandPool(device, pool, nullptr);
          }
        }

        VkDevice device{VK_NULL_HANDLE};
        VkCommandPool pool{VK_NULL_HANDLE};
      };

      static constexpr std::uint32_t kInvalidQueueFamily =
        std::numeric_limits<std::uint32_t>::max();

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
                                        VkDeviceSize byteCount, const void* initialData) const {
        StorageBuffer result;
        result.byteCount = std::max<VkDeviceSize>(1u, byteCount);

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = result.byteCount;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
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
      StorageBuffer createStorageBufferFromVector(VkDevice device, VkPhysicalDevice physicalDevice,
                                                  const std::vector<Record>& records) const {
        return createStorageBuffer(device, physicalDevice, byteCount<Record>(records.size()),
                                   records.empty() ? nullptr : records.data());
      }

      StorageBuffer createStorageBufferFromBytes(VkDevice device, VkPhysicalDevice physicalDevice,
                                                 const std::vector<std::uint8_t>& bytes) const {
        return createStorageBuffer(device, physicalDevice, static_cast<VkDeviceSize>(bytes.size()),
                                   bytes.empty() ? nullptr : bytes.data());
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

      VkDescriptorPool createDescriptorPool(VkDevice device, std::uint32_t descriptorCount) const {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = descriptorCount;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
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

      void recordPathLoopAndOptionalResolve(VkCommandBuffer commandBuffer,
                                            VkPipeline pathLoopPipeline, VkPipeline resolvePipeline,
                                            VkPipelineLayout pipelineLayout,
                                            VkDescriptorSet descriptorSet, std::uint32_t pathCount,
                                            std::uint32_t resolvedDisplayPixels,
                                            bool captureResolvedDisplay) const {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        check(vkBeginCommandBuffer(commandBuffer, &beginInfo),
              "Vulkan diffuse path-loop command buffer begin");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pathLoopPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1,
                                &descriptorSet, 0, nullptr);
        const std::uint32_t groupCount =
          std::max(1u, (pathCount + kDiffusePathLoopLocalSizeX - 1u) / kDiffusePathLoopLocalSizeX);
        vkCmdDispatch(commandBuffer, groupCount, 1, 1);
        if (captureResolvedDisplay) {
          VkMemoryBarrier barrier{};
          barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
          barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
          barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
          vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0,
                               nullptr);
          vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, resolvePipeline);
          const std::uint32_t resolveGroupCount =
            std::max(1u, (resolvedDisplayPixels + kDiffusePathLoopLocalSizeX - 1u) /
                           kDiffusePathLoopLocalSizeX);
          vkCmdDispatch(commandBuffer, resolveGroupCount, 1, 1);
        }
        check(vkEndCommandBuffer(commandBuffer), "Vulkan diffuse path-loop command buffer end");
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
    };
#endif
  }

  bool VulkanGpuDiffusePathLoopKernel::deviceAvailable() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanDiffusePathLoopRuntime().deviceAvailable();
#else
    return false;
#endif
  }

  std::string VulkanGpuDiffusePathLoopKernel::deviceUnavailableReason() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanDiffusePathLoopRuntime().deviceUnavailableReason();
#else
    return "Vulkan diffuse path-loop backend is not enabled in this build";
#endif
  }

  bool VulkanGpuDiffusePathLoopKernel::launchPathAvailable() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanDiffusePathLoopRuntime().launchPathAvailable();
#else
    return false;
#endif
  }

  std::string VulkanGpuDiffusePathLoopKernel::launchPathUnavailableReason() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanDiffusePathLoopRuntime().launchPathUnavailableReason();
#else
    return "Vulkan diffuse path-loop backend is not enabled in this build";
#endif
  }

  VulkanGpuDiffusePathLoopKernelResult VulkanGpuDiffusePathLoopKernel::runAllMissPathLoop(
    const GpuDiffusePathLoopLaunchPlan& plan,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
    bool capturePlatformAccumulation, bool captureResolvedDisplay) const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanDiffusePathLoopRuntime().runAllMissPathLoop(
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
