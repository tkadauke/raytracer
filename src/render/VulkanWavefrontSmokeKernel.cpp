#include "render/VulkanWavefrontSmokeKernel.h"

#include "render/GpuIntersectionScene.h"
#include "render/WavefrontIntersectionBackend.h"

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
#include "render/VulkanWavefrontShaders.generated.h"
#include "render/VulkanWavefrontTriangleAny.generated.h"
#include "render/VulkanWavefrontTriangleClosest.generated.h"

#include <vulkan/vulkan.h>
#endif

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace render {
  namespace {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    class VulkanSmokeRuntime final {
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

        InstanceGuard guard;
        guard.instance = instance;

        const DeviceSelection selection = selectDevice(instance);
        if (selection.device == VK_NULL_HANDLE) {
          return "Vulkan loader found no physical device with a compute queue";
        }
        return "";
      }

      bool renderPathAvailable() const {
        return renderPathUnavailableReason().empty();
      }

      std::string renderPathUnavailableReason() const {
        try {
          VkInstance instance = createInstance();
          InstanceGuard instanceGuard;
          instanceGuard.instance = instance;

          const DeviceSelection selection = selectDevice(instance);
          if (selection.device == VK_NULL_HANDLE) {
            return "Vulkan render-path probe found no physical device with a compute queue";
          }

          const float queuePriority = 1.0f;
          VkDeviceQueueCreateInfo queueCreateInfo{};
          queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
          queueCreateInfo.queueFamilyIndex = selection.queueFamily;
          queueCreateInfo.queueCount = 1;
          queueCreateInfo.pQueuePriorities = &queuePriority;

          VkDeviceCreateInfo deviceCreateInfo{};
          deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
          deviceCreateInfo.queueCreateInfoCount = 1;
          deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

          VkDevice device = VK_NULL_HANDLE;
          check(vkCreateDevice(selection.device, &deviceCreateInfo, nullptr, &device),
                "Vulkan wavefront render-path logical device creation");
          DeviceGuard deviceGuard;
          deviceGuard.device = device;

          VkShaderModule closestShader = createShaderModule(
            device, triangleClosestHitShaderSpirv().data(), triangleClosestHitShaderSpirv().size());
          ShaderGuard closestShaderGuard;
          closestShaderGuard.device = device;
          closestShaderGuard.shaderModule = closestShader;

          VkShaderModule anyShader = createShaderModule(device, triangleAnyHitShaderSpirv().data(),
                                                        triangleAnyHitShaderSpirv().size());
          ShaderGuard anyShaderGuard;
          anyShaderGuard.device = device;
          anyShaderGuard.shaderModule = anyShader;

          VkDescriptorSetLayout descriptorLayout = createDescriptorLayout(device, 13);
          DescriptorLayoutGuard descriptorLayoutGuard;
          descriptorLayoutGuard.device = device;
          descriptorLayoutGuard.layout = descriptorLayout;

          VkPipelineLayout pipelineLayout = createPipelineLayout(device, descriptorLayout);
          PipelineLayoutGuard pipelineLayoutGuard;
          pipelineLayoutGuard.device = device;
          pipelineLayoutGuard.layout = pipelineLayout;

          VkPipeline closestPipeline = createPipeline(device, closestShader, pipelineLayout);
          PipelineGuard closestPipelineGuard;
          closestPipelineGuard.device = device;
          closestPipelineGuard.pipeline = closestPipeline;

          VkPipeline anyPipeline = createPipeline(device, anyShader, pipelineLayout);
          PipelineGuard anyPipelineGuard;
          anyPipelineGuard.device = device;
          anyPipelineGuard.pipeline = anyPipeline;
          return "";
        } catch (const std::runtime_error& e) {
          return e.what();
        }
      }

      std::vector<std::uint32_t>
      runDummyHitMissKernel(const std::vector<std::uint32_t>& rayIds) const {
        if (rayIds.size() > std::numeric_limits<std::uint32_t>::max()) {
          throw std::runtime_error("Vulkan wavefront smoke kernel ray batch is too large");
        }

        VkInstance instance = createInstance();
        InstanceGuard instanceGuard;
        instanceGuard.instance = instance;

        const DeviceSelection selection = selectDevice(instance);
        if (selection.device == VK_NULL_HANDLE) {
          throw std::runtime_error("Vulkan wavefront smoke kernel requires a compute device");
        }

        const float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = selection.queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

        VkDevice device = VK_NULL_HANDLE;
        check(vkCreateDevice(selection.device, &deviceCreateInfo, nullptr, &device),
              "Vulkan wavefront smoke logical device creation");
        DeviceGuard deviceGuard;
        deviceGuard.device = device;

        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, selection.queueFamily, 0, &queue);

        const VkDeviceSize byteCount =
          static_cast<VkDeviceSize>(rayIds.size() * sizeof(std::uint32_t));
        SmokeBuffer inputBuffer =
          createStorageBuffer(device, selection.device, byteCount, rayIds.data());
        SmokeBuffer outputBuffer =
          createStorageBuffer(device, selection.device, byteCount, nullptr);
        BufferGuard bufferGuard;
        bufferGuard.device = device;
        bufferGuard.input = inputBuffer;
        bufferGuard.output = outputBuffer;

        const auto& shader = dummyComputeShaderSpirv();
        VkShaderModuleCreateInfo shaderInfo{};
        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = shader.size() * sizeof(std::uint32_t);
        shaderInfo.pCode = shader.data();

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        check(vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule),
              "Vulkan wavefront smoke shader module creation");
        ShaderGuard shaderGuard;
        shaderGuard.device = device;
        shaderGuard.shaderModule = shaderModule;

        VkDescriptorSetLayout descriptorLayout = createDescriptorLayout(device, 2);
        DescriptorLayoutGuard descriptorLayoutGuard;
        descriptorLayoutGuard.device = device;
        descriptorLayoutGuard.layout = descriptorLayout;

        VkPipelineLayout pipelineLayout = createPipelineLayout(device, descriptorLayout);
        PipelineLayoutGuard pipelineLayoutGuard;
        pipelineLayoutGuard.device = device;
        pipelineLayoutGuard.layout = pipelineLayout;

        VkPipeline pipeline = createPipeline(device, shaderModule, pipelineLayout);
        PipelineGuard pipelineGuard;
        pipelineGuard.device = device;
        pipelineGuard.pipeline = pipeline;

        VkDescriptorPool descriptorPool = createDescriptorPool(device, 2);
        DescriptorPoolGuard descriptorPoolGuard;
        descriptorPoolGuard.device = device;
        descriptorPoolGuard.pool = descriptorPool;

        VkDescriptorSet descriptorSet =
          allocateDescriptorSet(device, descriptorPool, descriptorLayout);
        updateDescriptorSet(device, descriptorSet,
                            std::vector<std::pair<VkBuffer, VkDeviceSize>>{
                              {inputBuffer.buffer, inputBuffer.byteCount},
                              {outputBuffer.buffer, outputBuffer.byteCount},
                            });

        VkCommandPool commandPool = createCommandPool(device, selection.queueFamily);
        CommandPoolGuard commandPoolGuard;
        commandPoolGuard.device = device;
        commandPoolGuard.pool = commandPool;

        VkCommandBuffer commandBuffer = allocateCommandBuffer(device, commandPool);
        recordDispatch(commandBuffer, pipeline, pipelineLayout, descriptorSet,
                       static_cast<std::uint32_t>(rayIds.size()));
        submitAndWait(queue, commandBuffer);

        return readBack(device, outputBuffer.memory, byteCount, rayIds.size());
      }

      VulkanWavefrontClosestHitKernelResult
      runTimedBasicClosestHitKernel(const GpuIntersectionSceneBuffers& scene,
                                    const std::vector<GpuIntersectionRay>& rays) const {
        if (rays.empty()) {
          return {};
        }
        if (!VulkanWavefrontIntersectionBackend::supportsPackedScene(scene)) {
          throw std::invalid_argument(
            "Vulkan basic closest-hit kernel requires an exact-primitive/static-transform scene");
        }
        if (rays.size() > std::numeric_limits<std::uint32_t>::max()) {
          throw std::runtime_error("Vulkan basic closest-hit ray batch is too large");
        }

        VulkanWavefrontClosestHitKernelResult result;
        const auto uploadStart = std::chrono::steady_clock::now();

        VkInstance instance = createInstance();
        InstanceGuard instanceGuard;
        instanceGuard.instance = instance;

        const DeviceSelection selection = selectDevice(instance);
        if (selection.device == VK_NULL_HANDLE) {
          throw std::runtime_error("Vulkan basic closest-hit kernel requires a compute device");
        }

        const float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = selection.queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

        VkDevice device = VK_NULL_HANDLE;
        check(vkCreateDevice(selection.device, &deviceCreateInfo, nullptr, &device),
              "Vulkan basic closest-hit logical device creation");
        DeviceGuard deviceGuard;
        deviceGuard.device = device;

        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, selection.queueFamily, 0, &queue);

        BufferVectorGuard bufferGuard;
        bufferGuard.device = device;
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.bvh));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.primitives));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.triangles));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.spheres));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.planes));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.rectangles));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.disks));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.openCylinders));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.tori));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.transforms));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, rays));

        const VkDeviceSize hitByteCount =
          byteCountForRecords<GpuIntersectionHitRecord>(rays.size());
        bufferGuard.buffers.push_back(
          createStorageBuffer(device, selection.device, hitByteCount, nullptr));

        const std::array<std::uint32_t, 12> counts{
          static_cast<std::uint32_t>(scene.bvh.size()),
          static_cast<std::uint32_t>(scene.primitives.size()),
          static_cast<std::uint32_t>(scene.triangles.size()),
          static_cast<std::uint32_t>(scene.spheres.size()),
          static_cast<std::uint32_t>(scene.planes.size()),
          static_cast<std::uint32_t>(scene.rectangles.size()),
          static_cast<std::uint32_t>(scene.disks.size()),
          static_cast<std::uint32_t>(scene.openCylinders.size()),
          static_cast<std::uint32_t>(rays.size()),
          static_cast<std::uint32_t>(scene.transforms.size()),
          static_cast<std::uint32_t>(scene.tori.size()),
          0u,
        };
        bufferGuard.buffers.push_back(
          createStorageBuffer(device, selection.device, sizeof(counts), counts.data()));

        const auto& shader = triangleClosestHitShaderSpirv();
        VkShaderModule shaderModule = createShaderModule(device, shader.data(), shader.size());
        ShaderGuard shaderGuard;
        shaderGuard.device = device;
        shaderGuard.shaderModule = shaderModule;

        VkDescriptorSetLayout descriptorLayout =
          createDescriptorLayout(device, static_cast<std::uint32_t>(bufferGuard.buffers.size()));
        DescriptorLayoutGuard descriptorLayoutGuard;
        descriptorLayoutGuard.device = device;
        descriptorLayoutGuard.layout = descriptorLayout;

        VkPipelineLayout pipelineLayout = createPipelineLayout(device, descriptorLayout);
        PipelineLayoutGuard pipelineLayoutGuard;
        pipelineLayoutGuard.device = device;
        pipelineLayoutGuard.layout = pipelineLayout;

        VkPipeline pipeline = createPipeline(device, shaderModule, pipelineLayout);
        PipelineGuard pipelineGuard;
        pipelineGuard.device = device;
        pipelineGuard.pipeline = pipeline;

        VkDescriptorPool descriptorPool =
          createDescriptorPool(device, static_cast<std::uint32_t>(bufferGuard.buffers.size()));
        DescriptorPoolGuard descriptorPoolGuard;
        descriptorPoolGuard.device = device;
        descriptorPoolGuard.pool = descriptorPool;

        VkDescriptorSet descriptorSet =
          allocateDescriptorSet(device, descriptorPool, descriptorLayout);
        updateDescriptorSet(device, descriptorSet, bufferGuard.buffers);

        VkCommandPool commandPool = createCommandPool(device, selection.queueFamily);
        CommandPoolGuard commandPoolGuard;
        commandPoolGuard.device = device;
        commandPoolGuard.pool = commandPool;

        VkCommandBuffer commandBuffer = allocateCommandBuffer(device, commandPool);
        recordDispatch(commandBuffer, pipeline, pipelineLayout, descriptorSet,
                       static_cast<std::uint32_t>(rays.size()));
        const auto uploadEnd = std::chrono::steady_clock::now();

        const auto kernelStart = std::chrono::steady_clock::now();
        submitAndWait(queue, commandBuffer);
        const auto kernelEnd = std::chrono::steady_clock::now();

        const auto readbackStart = std::chrono::steady_clock::now();
        result.hits = readBackRecords<GpuIntersectionHitRecord>(
          device, bufferGuard.buffers[11].memory, hitByteCount, rays.size(),
          "Vulkan basic closest-hit output buffer mapping");
        const auto readbackEnd = std::chrono::steady_clock::now();

        result.timing.uploadSeconds = secondsBetween(uploadStart, uploadEnd);
        result.timing.kernelSeconds = secondsBetween(kernelStart, kernelEnd);
        result.timing.readbackSeconds = secondsBetween(readbackStart, readbackEnd);
        result.timing.recordExecutionPath("vulkan");
        return result;
      }

      VulkanWavefrontAnyHitKernelResult
      runTimedBasicAnyHitKernel(const GpuIntersectionSceneBuffers& scene,
                                const std::vector<GpuIntersectionRay>& rays) const {
        if (rays.empty()) {
          return {};
        }
        if (!VulkanWavefrontIntersectionBackend::supportsPackedScene(scene)) {
          throw std::invalid_argument(
            "Vulkan basic any-hit kernel requires an exact-primitive/static-transform scene");
        }
        if (rays.size() > std::numeric_limits<std::uint32_t>::max()) {
          throw std::runtime_error("Vulkan basic any-hit ray batch is too large");
        }

        VulkanWavefrontAnyHitKernelResult result;
        const auto uploadStart = std::chrono::steady_clock::now();

        VkInstance instance = createInstance();
        InstanceGuard instanceGuard;
        instanceGuard.instance = instance;

        const DeviceSelection selection = selectDevice(instance);
        if (selection.device == VK_NULL_HANDLE) {
          throw std::runtime_error("Vulkan basic any-hit kernel requires a compute device");
        }

        const float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = selection.queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

        VkDevice device = VK_NULL_HANDLE;
        check(vkCreateDevice(selection.device, &deviceCreateInfo, nullptr, &device),
              "Vulkan basic any-hit logical device creation");
        DeviceGuard deviceGuard;
        deviceGuard.device = device;

        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, selection.queueFamily, 0, &queue);

        BufferVectorGuard bufferGuard;
        bufferGuard.device = device;
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.bvh));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.primitives));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.triangles));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.spheres));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.planes));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.rectangles));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.disks));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.openCylinders));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.tori));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, scene.transforms));
        bufferGuard.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, rays));

        const VkDeviceSize recordByteCount =
          byteCountForRecords<GpuIntersectionOcclusionRecord>(rays.size());
        bufferGuard.buffers.push_back(
          createStorageBuffer(device, selection.device, recordByteCount, nullptr));

        const std::array<std::uint32_t, 12> counts{
          static_cast<std::uint32_t>(scene.bvh.size()),
          static_cast<std::uint32_t>(scene.primitives.size()),
          static_cast<std::uint32_t>(scene.triangles.size()),
          static_cast<std::uint32_t>(scene.spheres.size()),
          static_cast<std::uint32_t>(scene.planes.size()),
          static_cast<std::uint32_t>(scene.rectangles.size()),
          static_cast<std::uint32_t>(scene.disks.size()),
          static_cast<std::uint32_t>(scene.openCylinders.size()),
          static_cast<std::uint32_t>(rays.size()),
          static_cast<std::uint32_t>(scene.transforms.size()),
          static_cast<std::uint32_t>(scene.tori.size()),
          0u,
        };
        bufferGuard.buffers.push_back(
          createStorageBuffer(device, selection.device, sizeof(counts), counts.data()));

        const auto& shader = triangleAnyHitShaderSpirv();
        VkShaderModule shaderModule = createShaderModule(device, shader.data(), shader.size());
        ShaderGuard shaderGuard;
        shaderGuard.device = device;
        shaderGuard.shaderModule = shaderModule;

        VkDescriptorSetLayout descriptorLayout =
          createDescriptorLayout(device, static_cast<std::uint32_t>(bufferGuard.buffers.size()));
        DescriptorLayoutGuard descriptorLayoutGuard;
        descriptorLayoutGuard.device = device;
        descriptorLayoutGuard.layout = descriptorLayout;

        VkPipelineLayout pipelineLayout = createPipelineLayout(device, descriptorLayout);
        PipelineLayoutGuard pipelineLayoutGuard;
        pipelineLayoutGuard.device = device;
        pipelineLayoutGuard.layout = pipelineLayout;

        VkPipeline pipeline = createPipeline(device, shaderModule, pipelineLayout);
        PipelineGuard pipelineGuard;
        pipelineGuard.device = device;
        pipelineGuard.pipeline = pipeline;

        VkDescriptorPool descriptorPool =
          createDescriptorPool(device, static_cast<std::uint32_t>(bufferGuard.buffers.size()));
        DescriptorPoolGuard descriptorPoolGuard;
        descriptorPoolGuard.device = device;
        descriptorPoolGuard.pool = descriptorPool;

        VkDescriptorSet descriptorSet =
          allocateDescriptorSet(device, descriptorPool, descriptorLayout);
        updateDescriptorSet(device, descriptorSet, bufferGuard.buffers);

        VkCommandPool commandPool = createCommandPool(device, selection.queueFamily);
        CommandPoolGuard commandPoolGuard;
        commandPoolGuard.device = device;
        commandPoolGuard.pool = commandPool;

        VkCommandBuffer commandBuffer = allocateCommandBuffer(device, commandPool);
        recordDispatch(commandBuffer, pipeline, pipelineLayout, descriptorSet,
                       static_cast<std::uint32_t>(rays.size()));
        const auto uploadEnd = std::chrono::steady_clock::now();

        const auto kernelStart = std::chrono::steady_clock::now();
        submitAndWait(queue, commandBuffer);
        const auto kernelEnd = std::chrono::steady_clock::now();

        const auto readbackStart = std::chrono::steady_clock::now();
        result.records = readBackRecords<GpuIntersectionOcclusionRecord>(
          device, bufferGuard.buffers[11].memory, recordByteCount, rays.size(),
          "Vulkan basic any-hit output buffer mapping");
        const auto readbackEnd = std::chrono::steady_clock::now();

        result.timing.uploadSeconds = secondsBetween(uploadStart, uploadEnd);
        result.timing.kernelSeconds = secondsBetween(kernelStart, kernelEnd);
        result.timing.readbackSeconds = secondsBetween(readbackStart, readbackEnd);
        result.timing.recordExecutionPath("vulkan");
        return result;
      }

    private:
      struct DeviceSelection {
        VkPhysicalDevice device{VK_NULL_HANDLE};
        std::uint32_t queueFamily{kInvalidQueueFamily};
      };

      struct SmokeBuffer {
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

      struct BufferGuard {
        ~BufferGuard() {
          if (input.buffer) {
            vkDestroyBuffer(device, input.buffer, nullptr);
          }
          if (input.memory) {
            vkFreeMemory(device, input.memory, nullptr);
          }
          if (output.buffer) {
            vkDestroyBuffer(device, output.buffer, nullptr);
          }
          if (output.memory) {
            vkFreeMemory(device, output.memory, nullptr);
          }
        }

        VkDevice device{VK_NULL_HANDLE};
        SmokeBuffer input;
        SmokeBuffer output;
      };

      struct BufferVectorGuard {
        ~BufferVectorGuard() {
          for (SmokeBuffer& buffer : buffers) {
            if (buffer.buffer) {
              vkDestroyBuffer(device, buffer.buffer, nullptr);
            }
            if (buffer.memory) {
              vkFreeMemory(device, buffer.memory, nullptr);
            }
          }
        }

        VkDevice device{VK_NULL_HANDLE};
        std::vector<SmokeBuffer> buffers;
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

      const std::array<std::uint32_t, vulkan_shaders::smokeHitMissShaderSpirv.size()>&
      dummyComputeShaderSpirv() const {
        return vulkan_shaders::smokeHitMissShaderSpirv;
      }

      const std::array<std::uint32_t, vulkan_shaders::triangleClosestHitShaderSpirv.size()>&
      triangleClosestHitShaderSpirv() const {
        return vulkan_shaders::triangleClosestHitShaderSpirv;
      }

      const std::array<std::uint32_t, vulkan_shaders::triangleAnyHitShaderSpirv.size()>&
      triangleAnyHitShaderSpirv() const {
        return vulkan_shaders::triangleAnyHitShaderSpirv;
      }

      void check(VkResult result, const char* operation) const {
        if (result != VK_SUCCESS) {
          throw std::runtime_error(std::string(operation) + " failed");
        }
      }

      double secondsBetween(std::chrono::steady_clock::time_point start,
                            std::chrono::steady_clock::time_point end) const {
        return std::chrono::duration<double>(end - start).count();
      }

      VkInstance createInstance() const {
        VkApplicationInfo applicationInfo{};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "raytracer Vulkan wavefront smoke probe";
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pEngineName = "raytracer";
        applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &applicationInfo;

        VkInstance instance = VK_NULL_HANDLE;
        check(vkCreateInstance(&createInfo, nullptr, &instance),
              "Vulkan wavefront smoke instance creation");
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
        throw std::runtime_error(
          "Vulkan wavefront smoke kernel requires host-coherent buffer memory");
      }

      SmokeBuffer createStorageBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                                      VkDeviceSize byteCount, const void* initialData) const {
        SmokeBuffer result;
        result.byteCount = byteCount;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = byteCount;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device, &bufferInfo, nullptr, &result.buffer),
              "Vulkan wavefront smoke buffer creation");

        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, result.buffer, &requirements);

        VkMemoryAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = requirements.size;
        allocateInfo.memoryTypeIndex =
          findHostVisibleMemoryType(physicalDevice, requirements.memoryTypeBits);
        check(vkAllocateMemory(device, &allocateInfo, nullptr, &result.memory),
              "Vulkan wavefront smoke buffer memory allocation");
        check(vkBindBufferMemory(device, result.buffer, result.memory, 0),
              "Vulkan wavefront smoke buffer binding");

        if (initialData) {
          void* mapped = nullptr;
          check(vkMapMemory(device, result.memory, 0, byteCount, 0, &mapped),
                "Vulkan wavefront smoke input buffer mapping");
          std::memcpy(mapped, initialData, static_cast<std::size_t>(byteCount));
          vkUnmapMemory(device, result.memory);
        }
        return result;
      }

      template<typename Record>
      VkDeviceSize byteCountForRecords(std::size_t recordCount) const {
        if (recordCount >
            std::numeric_limits<VkDeviceSize>::max() / static_cast<VkDeviceSize>(sizeof(Record))) {
          throw std::runtime_error("Vulkan wavefront buffer is too large");
        }
        return static_cast<VkDeviceSize>(recordCount) * static_cast<VkDeviceSize>(sizeof(Record));
      }

      template<typename Record>
      SmokeBuffer createStorageBufferFromVector(VkDevice device, VkPhysicalDevice physicalDevice,
                                                const std::vector<Record>& records) const {
        if (records.empty()) {
          return createStorageBuffer(device, physicalDevice,
                                     static_cast<VkDeviceSize>(sizeof(Record)), nullptr);
        }
        const VkDeviceSize byteCount = byteCountForRecords<Record>(records.size());
        return createStorageBuffer(device, physicalDevice, byteCount, records.data());
      }

      VkShaderModule createShaderModule(VkDevice device, const std::uint32_t* words,
                                        std::size_t wordCount) const {
        VkShaderModuleCreateInfo shaderInfo{};
        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = wordCount * sizeof(std::uint32_t);
        shaderInfo.pCode = words;

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        check(vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule),
              "Vulkan wavefront shader module creation");
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
          "Vulkan wavefront smoke descriptor layout creation");
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
              "Vulkan wavefront smoke pipeline layout creation");
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
          "Vulkan wavefront smoke compute pipeline creation");
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
              "Vulkan wavefront smoke descriptor pool creation");
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
              "Vulkan wavefront smoke descriptor set allocation");
        return descriptorSet;
      }

      void
      updateDescriptorSet(VkDevice device, VkDescriptorSet descriptorSet,
                          const std::vector<std::pair<VkBuffer, VkDeviceSize>>& buffers) const {
        std::vector<VkDescriptorBufferInfo> descriptors(buffers.size());
        std::vector<VkWriteDescriptorSet> descriptorWrites(buffers.size());
        for (std::size_t index = 0; index != buffers.size(); ++index) {
          descriptors[index].buffer = buffers[index].first;
          descriptors[index].offset = 0;
          descriptors[index].range = buffers[index].second;

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

      void updateDescriptorSet(VkDevice device, VkDescriptorSet descriptorSet,
                               const std::vector<SmokeBuffer>& buffers) const {
        std::vector<std::pair<VkBuffer, VkDeviceSize>> descriptors;
        descriptors.reserve(buffers.size());
        for (const SmokeBuffer& buffer : buffers) {
          descriptors.push_back({buffer.buffer, buffer.byteCount});
        }
        updateDescriptorSet(device, descriptorSet, descriptors);
      }

      VkCommandPool createCommandPool(VkDevice device, std::uint32_t queueFamily) const {
        VkCommandPoolCreateInfo commandPoolInfo{};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.queueFamilyIndex = queueFamily;

        VkCommandPool commandPool = VK_NULL_HANDLE;
        check(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool),
              "Vulkan wavefront smoke command pool creation");
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
              "Vulkan wavefront smoke command buffer allocation");
        return commandBuffer;
      }

      void recordDispatch(VkCommandBuffer commandBuffer, VkPipeline pipeline,
                          VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                          std::uint32_t rayCount) const {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        check(vkBeginCommandBuffer(commandBuffer, &beginInfo),
              "Vulkan wavefront smoke command buffer begin");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1,
                                &descriptorSet, 0, nullptr);
        vkCmdDispatch(commandBuffer, rayCount, 1, 1);
        check(vkEndCommandBuffer(commandBuffer), "Vulkan wavefront smoke command buffer end");
      }

      void submitAndWait(VkQueue queue, VkCommandBuffer commandBuffer) const {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        check(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE),
              "Vulkan wavefront smoke queue submit");
        check(vkQueueWaitIdle(queue), "Vulkan wavefront smoke queue wait");
      }

      std::vector<std::uint32_t> readBack(VkDevice device, VkDeviceMemory outputMemory,
                                          VkDeviceSize byteCount, std::size_t resultCount) const {
        return readBackRecords<std::uint32_t>(device, outputMemory, byteCount, resultCount,
                                              "Vulkan wavefront smoke output buffer mapping");
      }

      template<typename Record>
      std::vector<Record> readBackRecords(VkDevice device, VkDeviceMemory outputMemory,
                                          VkDeviceSize byteCount, std::size_t resultCount,
                                          const char* operation) const {
        std::vector<Record> results(resultCount);
        void* mapped = nullptr;
        check(vkMapMemory(device, outputMemory, 0, byteCount, 0, &mapped), operation);
        std::memcpy(results.data(), mapped, static_cast<std::size_t>(byteCount));
        vkUnmapMemory(device, outputMemory);
        return results;
      }
    };
#endif
  }

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
  struct VulkanPreparedSmokeBuffer {
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkDeviceSize byteCount{0};
  };

  struct VulkanWavefrontPreparedRayBatch::Private {
    std::shared_ptr<const void> sceneLifetime;
    VkDevice device{VK_NULL_HANDLE};
    VulkanPreparedSmokeBuffer rays;
    VulkanPreparedSmokeBuffer counts;
    std::uint64_t rayCount{0};

    ~Private() {
      destroy(rays);
      destroy(counts);
    }

    void destroy(VulkanPreparedSmokeBuffer& buffer) const {
      if (device && buffer.buffer) {
        vkDestroyBuffer(device, buffer.buffer, nullptr);
        buffer.buffer = VK_NULL_HANDLE;
      }
      if (device && buffer.memory) {
        vkFreeMemory(device, buffer.memory, nullptr);
        buffer.memory = VK_NULL_HANDLE;
      }
    }
  };
#else
  struct VulkanWavefrontPreparedRayBatch::Private {
    std::uint64_t rayCount{0};
  };
#endif

  VulkanWavefrontPreparedRayBatch::VulkanWavefrontPreparedRayBatch()
      : p(std::make_unique<Private>()) {
  }

  VulkanWavefrontPreparedRayBatch::~VulkanWavefrontPreparedRayBatch() = default;

  std::uint64_t VulkanWavefrontPreparedRayBatch::rayCount() const {
    return p->rayCount;
  }

  std::uint64_t VulkanWavefrontPreparedRayBatch::packedRayBytes() const {
    return p->rayCount * sizeof(GpuIntersectionRay);
  }

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
  struct VulkanWavefrontPreparedScene::Private {
    explicit Private(const GpuIntersectionSceneBuffers& scene) {
      try {
        if (!VulkanWavefrontIntersectionBackend::supportsPackedScene(scene)) {
          throw std::invalid_argument(
            "Vulkan prepared wavefront scene requires an exact-primitive/static-transform scene");
        }

        instance = createInstance();
        const DeviceSelection selection = selectDevice(instance);
        if (selection.device == VK_NULL_HANDLE) {
          throw std::runtime_error("Vulkan prepared wavefront scene requires a compute device");
        }
        physicalDevice = selection.device;
        queueFamily = selection.queueFamily;
        device = createDevice(physicalDevice, queueFamily);
        vkGetDeviceQueue(device, queueFamily, 0, &queue);

        sceneBuffers.reserve(10);
        sceneBuffers.push_back(createStorageBufferFromVector(scene.bvh));
        sceneBuffers.push_back(createStorageBufferFromVector(scene.primitives));
        sceneBuffers.push_back(createStorageBufferFromVector(scene.triangles));
        sceneBuffers.push_back(createStorageBufferFromVector(scene.spheres));
        sceneBuffers.push_back(createStorageBufferFromVector(scene.planes));
        sceneBuffers.push_back(createStorageBufferFromVector(scene.rectangles));
        sceneBuffers.push_back(createStorageBufferFromVector(scene.disks));
        sceneBuffers.push_back(createStorageBufferFromVector(scene.openCylinders));
        sceneBuffers.push_back(createStorageBufferFromVector(scene.tori));
        sceneBuffers.push_back(createStorageBufferFromVector(scene.transforms));

        sceneCounts0 = {
          static_cast<std::uint32_t>(scene.bvh.size()),
          static_cast<std::uint32_t>(scene.primitives.size()),
          static_cast<std::uint32_t>(scene.triangles.size()),
          static_cast<std::uint32_t>(scene.spheres.size()),
        };
        sceneCounts1 = {
          static_cast<std::uint32_t>(scene.planes.size()),
          static_cast<std::uint32_t>(scene.rectangles.size()),
          static_cast<std::uint32_t>(scene.disks.size()),
          static_cast<std::uint32_t>(scene.openCylinders.size()),
        };
        transformCount = static_cast<std::uint32_t>(scene.transforms.size());
        torusCount = static_cast<std::uint32_t>(scene.tori.size());

        closestShader = createShaderModule(vulkan_shaders::triangleClosestHitShaderSpirv.data(),
                                           vulkan_shaders::triangleClosestHitShaderSpirv.size());
        anyShader = createShaderModule(vulkan_shaders::triangleAnyHitShaderSpirv.data(),
                                       vulkan_shaders::triangleAnyHitShaderSpirv.size());
        descriptorLayout = createDescriptorLayout(13);
        pipelineLayout = createPipelineLayout(descriptorLayout);
        closestPipeline = createPipeline(closestShader);
        anyPipeline = createPipeline(anyShader);
      } catch (...) {
        cleanup();
        throw;
      }
    }

    ~Private() {
      cleanup();
    }

    void cleanup() {
      if (device) {
        for (const auto& buffers : queryBufferPool) {
          destroy(*buffers);
        }
        queryBufferPool.clear();
        if (closestPipeline) {
          vkDestroyPipeline(device, closestPipeline, nullptr);
          closestPipeline = VK_NULL_HANDLE;
        }
        if (anyPipeline) {
          vkDestroyPipeline(device, anyPipeline, nullptr);
          anyPipeline = VK_NULL_HANDLE;
        }
        if (pipelineLayout) {
          vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
          pipelineLayout = VK_NULL_HANDLE;
        }
        if (descriptorLayout) {
          vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
          descriptorLayout = VK_NULL_HANDLE;
        }
        if (closestShader) {
          vkDestroyShaderModule(device, closestShader, nullptr);
          closestShader = VK_NULL_HANDLE;
        }
        if (anyShader) {
          vkDestroyShaderModule(device, anyShader, nullptr);
          anyShader = VK_NULL_HANDLE;
        }
        for (SmokeBuffer& buffer : sceneBuffers) {
          destroy(buffer);
        }
        sceneBuffers.clear();
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
      }
      if (instance) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
      }
    }

    VulkanWavefrontClosestHitKernelResult
    runClosest(const std::vector<GpuIntersectionRay>& rays) const {
      VulkanWavefrontClosestHitKernelResult result;
      if (rays.empty()) {
        return result;
      }
      auto dispatchResult = dispatchRecords<GpuIntersectionHitRecord>(
        rays, closestPipeline, "Vulkan prepared closest-hit",
        "Vulkan prepared closest-hit output buffer mapping");
      result.hits = std::move(dispatchResult.records);
      result.timing = dispatchResult.timing;
      result.timing.recordExecutionPath("vulkan");
      return result;
    }

    VulkanWavefrontAnyHitKernelResult runAny(const std::vector<GpuIntersectionRay>& rays) const {
      VulkanWavefrontAnyHitKernelResult result;
      if (rays.empty()) {
        return result;
      }
      auto dispatchResult = dispatchRecords<GpuIntersectionOcclusionRecord>(
        rays, anyPipeline, "Vulkan prepared any-hit",
        "Vulkan prepared any-hit output buffer mapping");
      result.records = std::move(dispatchResult.records);
      result.timing = dispatchResult.timing;
      result.timing.recordExecutionPath("vulkan");
      return result;
    }

    VulkanWavefrontClosestHitKernelResult runClosestPrepared(std::uint64_t rayCount,
                                                             const SmokeBuffer& rayBuffer,
                                                             const SmokeBuffer& countBuffer) const {
      VulkanWavefrontClosestHitKernelResult result;
      if (rayCount == 0) {
        return result;
      }
      auto dispatchResult = dispatchPreparedRecords<GpuIntersectionHitRecord>(
        rayCount, rayBuffer, countBuffer, closestPipeline, "Vulkan prepared closest-hit",
        "Vulkan prepared closest-hit output buffer mapping");
      result.hits = std::move(dispatchResult.records);
      result.timing = dispatchResult.timing;
      result.timing.recordExecutionPath("vulkan");
      return result;
    }

    VulkanWavefrontAnyHitKernelResult runAnyPrepared(std::uint64_t rayCount,
                                                     const SmokeBuffer& rayBuffer,
                                                     const SmokeBuffer& countBuffer) const {
      VulkanWavefrontAnyHitKernelResult result;
      if (rayCount == 0) {
        return result;
      }
      auto dispatchResult = dispatchPreparedRecords<GpuIntersectionOcclusionRecord>(
        rayCount, rayBuffer, countBuffer, anyPipeline, "Vulkan prepared any-hit",
        "Vulkan prepared any-hit output buffer mapping");
      result.records = std::move(dispatchResult.records);
      result.timing = dispatchResult.timing;
      result.timing.recordExecutionPath("vulkan");
      return result;
    }

    struct DeviceSelection {
      VkPhysicalDevice device{VK_NULL_HANDLE};
      std::uint32_t queueFamily{kInvalidQueueFamily};
    };

    using SmokeBuffer = VulkanPreparedSmokeBuffer;

    struct QueryBuffers {
      SmokeBuffer rays;
      SmokeBuffer output;
      SmokeBuffer counts;
      VkCommandPool commandPool{VK_NULL_HANDLE};
      VkDeviceSize rayCapacityBytes{0};
      VkDeviceSize outputCapacityBytes{0};
      bool inUse{false};
    };

    struct QueryBufferLease {
      QueryBufferLease(const Private& owner, QueryBuffers& buffers)
          : owner(&owner),
            buffers(&buffers) {
      }

      QueryBufferLease(const QueryBufferLease&) = delete;
      QueryBufferLease& operator=(const QueryBufferLease&) = delete;

      QueryBufferLease(QueryBufferLease&& other) noexcept
          : owner(other.owner),
            buffers(other.buffers) {
        other.owner = nullptr;
        other.buffers = nullptr;
      }

      QueryBufferLease& operator=(QueryBufferLease&& other) noexcept = delete;

      ~QueryBufferLease() {
        if (owner && buffers) {
          owner->releaseQueryBuffers(*buffers);
        }
      }

      QueryBuffers& get() const {
        return *buffers;
      }

      const Private* owner;
      QueryBuffers* buffers;
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

    template<typename Record>
    struct DispatchRecordsResult {
      std::vector<Record> records;
      WavefrontIntersectionQueryTiming timing;
    };

    static constexpr std::uint32_t kInvalidQueueFamily = std::numeric_limits<std::uint32_t>::max();

    template<typename Record>
    DispatchRecordsResult<Record> dispatchRecords(const std::vector<GpuIntersectionRay>& rays,
                                                  VkPipeline pipeline, const char* operation,
                                                  const char* readbackOperation) const {
      if (rays.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error(std::string(operation) + " ray batch is too large");
      }

      DispatchRecordsResult<Record> result;
      const auto uploadStart = std::chrono::steady_clock::now();
      const VkDeviceSize rayBytes = byteCountForRecords<GpuIntersectionRay>(rays.size());
      const VkDeviceSize outputBytes = byteCountForRecords<Record>(rays.size());

      const QueryBufferLease queryBuffers = acquireQueryBuffers();
      ensureQueryBufferCapacity(queryBuffers.get(), rayBytes, outputBytes);
      writeBuffer(queryBuffers.get().rays, rayBytes, rays.data(),
                  "Vulkan prepared wavefront ray buffer mapping");

      const std::array<std::uint32_t, 12> counts{
        sceneCounts0[0], sceneCounts0[1], sceneCounts0[2],
        sceneCounts0[3], sceneCounts1[0], sceneCounts1[1],
        sceneCounts1[2], sceneCounts1[3], static_cast<std::uint32_t>(rays.size()),
        transformCount,  torusCount,      0u,
      };
      writeBuffer(queryBuffers.get().counts, sizeof(counts), counts.data(),
                  "Vulkan prepared wavefront count buffer mapping");

      std::vector<std::pair<VkBuffer, VkDeviceSize>> descriptors;
      descriptors.reserve(13);
      for (const SmokeBuffer& buffer : sceneBuffers) {
        descriptors.push_back({buffer.buffer, buffer.byteCount});
      }
      descriptors.push_back({queryBuffers.get().rays.buffer, rayBytes});
      descriptors.push_back({queryBuffers.get().output.buffer, outputBytes});
      descriptors.push_back(
        {queryBuffers.get().counts.buffer, queryBuffers.get().counts.byteCount});

      DescriptorPoolGuard descriptorPool;
      descriptorPool.device = device;
      descriptorPool.pool = createDescriptorPool(13);
      VkDescriptorSet descriptorSet = allocateDescriptorSet(descriptorPool.pool);
      updateDescriptorSet(descriptorSet, descriptors);

      check(vkResetCommandPool(device, queryBuffers.get().commandPool, 0),
            "Vulkan prepared wavefront command pool reset");
      VkCommandBuffer commandBuffer = allocateCommandBuffer(queryBuffers.get().commandPool);
      recordDispatch(commandBuffer, pipeline, descriptorSet,
                     static_cast<std::uint32_t>(rays.size()));
      const auto uploadEnd = std::chrono::steady_clock::now();

      const auto kernelStart = std::chrono::steady_clock::now();
      submitAndWait(commandBuffer);
      const auto kernelEnd = std::chrono::steady_clock::now();

      const auto readbackStart = std::chrono::steady_clock::now();
      result.records = readBackRecords<Record>(queryBuffers.get().output.memory, outputBytes,
                                               rays.size(), readbackOperation);
      const auto readbackEnd = std::chrono::steady_clock::now();

      result.timing.uploadSeconds = secondsBetween(uploadStart, uploadEnd);
      result.timing.kernelSeconds = secondsBetween(kernelStart, kernelEnd);
      result.timing.readbackSeconds = secondsBetween(readbackStart, readbackEnd);
      return result;
    }

    template<typename Record>
    DispatchRecordsResult<Record>
    dispatchPreparedRecords(std::uint64_t rayCount, const SmokeBuffer& rayBuffer,
                            const SmokeBuffer& countBuffer, VkPipeline pipeline,
                            const char* operation, const char* readbackOperation) const {
      if (rayCount > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error(std::string(operation) + " ray batch is too large");
      }

      DispatchRecordsResult<Record> result;
      const auto uploadStart = std::chrono::steady_clock::now();
      const VkDeviceSize outputBytes =
        byteCountForRecords<Record>(static_cast<std::size_t>(rayCount));

      const QueryBufferLease queryBuffers = acquireQueryBuffers();
      ensureOutputBufferCapacity(queryBuffers.get(), outputBytes);

      std::vector<std::pair<VkBuffer, VkDeviceSize>> descriptors;
      descriptors.reserve(13);
      for (const SmokeBuffer& buffer : sceneBuffers) {
        descriptors.push_back({buffer.buffer, buffer.byteCount});
      }
      descriptors.push_back({rayBuffer.buffer, rayBuffer.byteCount});
      descriptors.push_back({queryBuffers.get().output.buffer, outputBytes});
      descriptors.push_back({countBuffer.buffer, countBuffer.byteCount});

      DescriptorPoolGuard descriptorPool;
      descriptorPool.device = device;
      descriptorPool.pool = createDescriptorPool(13);
      VkDescriptorSet descriptorSet = allocateDescriptorSet(descriptorPool.pool);
      updateDescriptorSet(descriptorSet, descriptors);

      check(vkResetCommandPool(device, queryBuffers.get().commandPool, 0),
            "Vulkan prepared wavefront command pool reset");
      VkCommandBuffer commandBuffer = allocateCommandBuffer(queryBuffers.get().commandPool);
      recordDispatch(commandBuffer, pipeline, descriptorSet, static_cast<std::uint32_t>(rayCount));
      const auto uploadEnd = std::chrono::steady_clock::now();

      const auto kernelStart = std::chrono::steady_clock::now();
      submitAndWait(commandBuffer);
      const auto kernelEnd = std::chrono::steady_clock::now();

      const auto readbackStart = std::chrono::steady_clock::now();
      result.records =
        readBackRecords<Record>(queryBuffers.get().output.memory, outputBytes,
                                static_cast<std::size_t>(rayCount), readbackOperation);
      const auto readbackEnd = std::chrono::steady_clock::now();

      result.timing.uploadSeconds = secondsBetween(uploadStart, uploadEnd);
      result.timing.kernelSeconds = secondsBetween(kernelStart, kernelEnd);
      result.timing.readbackSeconds = secondsBetween(readbackStart, readbackEnd);
      return result;
    }

    QueryBufferLease acquireQueryBuffers() const {
      std::lock_guard<std::mutex> lock(queryBufferMutex);
      for (const auto& buffers : queryBufferPool) {
        if (!buffers->inUse) {
          buffers->inUse = true;
          return QueryBufferLease(*this, *buffers);
        }
      }

      auto buffers = std::make_unique<QueryBuffers>();
      buffers->commandPool = createCommandPool();
      buffers->inUse = true;
      queryBufferPool.push_back(std::move(buffers));
      return QueryBufferLease(*this, *queryBufferPool.back());
    }

    void releaseQueryBuffers(QueryBuffers& buffers) const {
      std::lock_guard<std::mutex> lock(queryBufferMutex);
      buffers.inUse = false;
    }

    void ensureQueryBufferCapacity(QueryBuffers& buffers, VkDeviceSize rayBytes,
                                   VkDeviceSize outputBytes) const {
      ensureBufferCapacity(buffers.rays, buffers.rayCapacityBytes, rayBytes);
      ensureBufferCapacity(buffers.output, buffers.outputCapacityBytes, outputBytes);
      if (!buffers.counts.buffer) {
        buffers.counts = createStorageBuffer(sizeof(std::array<std::uint32_t, 12>), nullptr);
      }
    }

    void ensureOutputBufferCapacity(QueryBuffers& buffers, VkDeviceSize outputBytes) const {
      ensureBufferCapacity(buffers.output, buffers.outputCapacityBytes, outputBytes);
    }

    void ensureBufferCapacity(SmokeBuffer& buffer, VkDeviceSize& capacityBytes,
                              VkDeviceSize requiredBytes) const {
      if (buffer.buffer && capacityBytes >= requiredBytes) {
        return;
      }
      destroy(buffer);
      buffer = createStorageBuffer(requiredBytes, nullptr);
      capacityBytes = requiredBytes;
    }

    void check(VkResult result, const char* operation) const {
      if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(operation) + " failed");
      }
    }

    double secondsBetween(std::chrono::steady_clock::time_point start,
                          std::chrono::steady_clock::time_point end) const {
      return std::chrono::duration<double>(end - start).count();
    }

    VkInstance createInstance() const {
      VkApplicationInfo applicationInfo{};
      applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
      applicationInfo.pApplicationName = "raytracer Vulkan prepared wavefront";
      applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
      applicationInfo.pEngineName = "raytracer";
      applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
      applicationInfo.apiVersion = VK_API_VERSION_1_0;

      VkInstanceCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
      createInfo.pApplicationInfo = &applicationInfo;

      VkInstance createdInstance = VK_NULL_HANDLE;
      check(vkCreateInstance(&createInfo, nullptr, &createdInstance),
            "Vulkan prepared wavefront instance creation");
      return createdInstance;
    }

    std::uint32_t computeQueueFamily(VkPhysicalDevice candidate) const {
      std::uint32_t queueFamilyCount = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, nullptr);
      if (queueFamilyCount == 0) {
        return kInvalidQueueFamily;
      }

      std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
      vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, queueFamilies.data());
      for (std::uint32_t index = 0; index != queueFamilyCount; ++index) {
        const VkQueueFamilyProperties& queueFamilyProperties = queueFamilies[index];
        if ((queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 &&
            queueFamilyProperties.queueCount > 0) {
          return index;
        }
      }
      return kInvalidQueueFamily;
    }

    DeviceSelection selectDevice(VkInstance sourceInstance) const {
      std::uint32_t deviceCount = 0;
      if (vkEnumeratePhysicalDevices(sourceInstance, &deviceCount, nullptr) != VK_SUCCESS ||
          deviceCount == 0) {
        return {};
      }

      std::vector<VkPhysicalDevice> devices(deviceCount, VK_NULL_HANDLE);
      if (vkEnumeratePhysicalDevices(sourceInstance, &deviceCount, devices.data()) != VK_SUCCESS) {
        return {};
      }

      for (VkPhysicalDevice candidate : devices) {
        const std::uint32_t candidateQueueFamily = computeQueueFamily(candidate);
        if (candidateQueueFamily != kInvalidQueueFamily) {
          return {candidate, candidateQueueFamily};
        }
      }
      return {};
    }

    VkDevice createDevice(VkPhysicalDevice selectedDevice,
                          std::uint32_t selectedQueueFamily) const {
      const float queuePriority = 1.0f;
      VkDeviceQueueCreateInfo queueCreateInfo{};
      queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = selectedQueueFamily;
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;

      VkDeviceCreateInfo deviceCreateInfo{};
      deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      deviceCreateInfo.queueCreateInfoCount = 1;
      deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

      VkDevice createdDevice = VK_NULL_HANDLE;
      check(vkCreateDevice(selectedDevice, &deviceCreateInfo, nullptr, &createdDevice),
            "Vulkan prepared wavefront logical device creation");
      return createdDevice;
    }

    std::uint32_t findHostVisibleMemoryType(std::uint32_t memoryTypeBits) const {
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
      throw std::runtime_error("Vulkan prepared wavefront requires host-coherent buffer memory");
    }

    SmokeBuffer createStorageBuffer(VkDeviceSize byteCount, const void* initialData) const {
      SmokeBuffer result;
      result.byteCount = byteCount;

      VkBufferCreateInfo bufferInfo{};
      bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      bufferInfo.size = byteCount;
      bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      check(vkCreateBuffer(device, &bufferInfo, nullptr, &result.buffer),
            "Vulkan prepared wavefront buffer creation");

      VkMemoryRequirements requirements{};
      vkGetBufferMemoryRequirements(device, result.buffer, &requirements);

      VkMemoryAllocateInfo allocateInfo{};
      allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocateInfo.allocationSize = requirements.size;
      allocateInfo.memoryTypeIndex = findHostVisibleMemoryType(requirements.memoryTypeBits);
      check(vkAllocateMemory(device, &allocateInfo, nullptr, &result.memory),
            "Vulkan prepared wavefront buffer memory allocation");
      check(vkBindBufferMemory(device, result.buffer, result.memory, 0),
            "Vulkan prepared wavefront buffer binding");

      if (initialData) {
        void* mapped = nullptr;
        check(vkMapMemory(device, result.memory, 0, byteCount, 0, &mapped),
              "Vulkan prepared wavefront input buffer mapping");
        std::memcpy(mapped, initialData, static_cast<std::size_t>(byteCount));
        vkUnmapMemory(device, result.memory);
      }
      return result;
    }

    void writeBuffer(const SmokeBuffer& buffer, VkDeviceSize byteCount, const void* data,
                     const char* operation) const {
      void* mapped = nullptr;
      check(vkMapMemory(device, buffer.memory, 0, byteCount, 0, &mapped), operation);
      std::memcpy(mapped, data, static_cast<std::size_t>(byteCount));
      vkUnmapMemory(device, buffer.memory);
    }

    template<typename Record>
    VkDeviceSize byteCountForRecords(std::size_t recordCount) const {
      if (recordCount >
          std::numeric_limits<VkDeviceSize>::max() / static_cast<VkDeviceSize>(sizeof(Record))) {
        throw std::runtime_error("Vulkan prepared wavefront buffer is too large");
      }
      return static_cast<VkDeviceSize>(recordCount) * static_cast<VkDeviceSize>(sizeof(Record));
    }

    template<typename Record>
    SmokeBuffer createStorageBufferFromVector(const std::vector<Record>& records) const {
      if (records.empty()) {
        return createStorageBuffer(static_cast<VkDeviceSize>(sizeof(Record)), nullptr);
      }
      return createStorageBuffer(byteCountForRecords<Record>(records.size()), records.data());
    }

    VkShaderModule createShaderModule(const std::uint32_t* words, std::size_t wordCount) const {
      VkShaderModuleCreateInfo shaderInfo{};
      shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
      shaderInfo.codeSize = wordCount * sizeof(std::uint32_t);
      shaderInfo.pCode = words;

      VkShaderModule shaderModule = VK_NULL_HANDLE;
      check(vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule),
            "Vulkan prepared wavefront shader module creation");
      return shaderModule;
    }

    VkDescriptorSetLayout createDescriptorLayout(std::uint32_t bindingCount) const {
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

      VkDescriptorSetLayout layout = VK_NULL_HANDLE;
      check(vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr, &layout),
            "Vulkan prepared wavefront descriptor layout creation");
      return layout;
    }

    VkPipelineLayout createPipelineLayout(VkDescriptorSetLayout layout) const {
      VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
      pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
      pipelineLayoutInfo.setLayoutCount = 1;
      pipelineLayoutInfo.pSetLayouts = &layout;

      VkPipelineLayout createdLayout = VK_NULL_HANDLE;
      check(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &createdLayout),
            "Vulkan prepared wavefront pipeline layout creation");
      return createdLayout;
    }

    VkPipeline createPipeline(VkShaderModule shaderModule) const {
      VkComputePipelineCreateInfo pipelineInfo{};
      pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
      pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
      pipelineInfo.stage.module = shaderModule;
      pipelineInfo.stage.pName = "main";
      pipelineInfo.layout = pipelineLayout;

      VkPipeline pipeline = VK_NULL_HANDLE;
      check(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline),
            "Vulkan prepared wavefront compute pipeline creation");
      return pipeline;
    }

    VkDescriptorPool createDescriptorPool(std::uint32_t descriptorCount) const {
      VkDescriptorPoolSize poolSize{};
      poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      poolSize.descriptorCount = descriptorCount;

      VkDescriptorPoolCreateInfo poolInfo{};
      poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
      poolInfo.maxSets = 1;
      poolInfo.poolSizeCount = 1;
      poolInfo.pPoolSizes = &poolSize;

      VkDescriptorPool pool = VK_NULL_HANDLE;
      check(vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool),
            "Vulkan prepared wavefront descriptor pool creation");
      return pool;
    }

    VkDescriptorSet allocateDescriptorSet(VkDescriptorPool descriptorPool) const {
      VkDescriptorSetAllocateInfo descriptorAllocateInfo{};
      descriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      descriptorAllocateInfo.descriptorPool = descriptorPool;
      descriptorAllocateInfo.descriptorSetCount = 1;
      descriptorAllocateInfo.pSetLayouts = &descriptorLayout;

      VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
      check(vkAllocateDescriptorSets(device, &descriptorAllocateInfo, &descriptorSet),
            "Vulkan prepared wavefront descriptor set allocation");
      return descriptorSet;
    }

    void updateDescriptorSet(VkDescriptorSet descriptorSet,
                             const std::vector<std::pair<VkBuffer, VkDeviceSize>>& buffers) const {
      std::vector<VkDescriptorBufferInfo> descriptors(buffers.size());
      std::vector<VkWriteDescriptorSet> descriptorWrites(buffers.size());
      for (std::size_t index = 0; index != buffers.size(); ++index) {
        descriptors[index].buffer = buffers[index].first;
        descriptors[index].offset = 0;
        descriptors[index].range = buffers[index].second;

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

    VkCommandPool createCommandPool() const {
      VkCommandPoolCreateInfo commandPoolInfo{};
      commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      commandPoolInfo.queueFamilyIndex = queueFamily;

      VkCommandPool pool = VK_NULL_HANDLE;
      check(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &pool),
            "Vulkan prepared wavefront command pool creation");
      return pool;
    }

    VkCommandBuffer allocateCommandBuffer(VkCommandPool sourceCommandPool) const {
      VkCommandBufferAllocateInfo commandAllocateInfo{};
      commandAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      commandAllocateInfo.commandPool = sourceCommandPool;
      commandAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      commandAllocateInfo.commandBufferCount = 1;

      VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
      check(vkAllocateCommandBuffers(device, &commandAllocateInfo, &commandBuffer),
            "Vulkan prepared wavefront command buffer allocation");
      return commandBuffer;
    }

    void recordDispatch(VkCommandBuffer commandBuffer, VkPipeline pipeline,
                        VkDescriptorSet descriptorSet, std::uint32_t rayCount) const {
      VkCommandBufferBeginInfo beginInfo{};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      check(vkBeginCommandBuffer(commandBuffer, &beginInfo),
            "Vulkan prepared wavefront command buffer begin");
      vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
      vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1,
                              &descriptorSet, 0, nullptr);
      vkCmdDispatch(commandBuffer, rayCount, 1, 1);
      check(vkEndCommandBuffer(commandBuffer), "Vulkan prepared wavefront command buffer end");
    }

    VkFence createFence() const {
      VkFenceCreateInfo fenceInfo{};
      fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

      VkFence fence = VK_NULL_HANDLE;
      check(vkCreateFence(device, &fenceInfo, nullptr, &fence),
            "Vulkan prepared wavefront fence creation");
      return fence;
    }

    void submitAndWait(VkCommandBuffer commandBuffer) const {
      const VkFence fence = createFence();
      VkSubmitInfo submitInfo{};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &commandBuffer;
      try {
        {
          std::lock_guard<std::mutex> lock(queueSubmitMutex);
          check(vkQueueSubmit(queue, 1, &submitInfo, fence),
                "Vulkan prepared wavefront queue submit");
        }
        check(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX),
              "Vulkan prepared wavefront fence wait");
      } catch (...) {
        vkDestroyFence(device, fence, nullptr);
        throw;
      }
      vkDestroyFence(device, fence, nullptr);
    }

    template<typename Record>
    std::vector<Record> readBackRecords(VkDeviceMemory outputMemory, VkDeviceSize byteCount,
                                        std::size_t resultCount, const char* operation) const {
      std::vector<Record> results(resultCount);
      void* mapped = nullptr;
      check(vkMapMemory(device, outputMemory, 0, byteCount, 0, &mapped), operation);
      std::memcpy(results.data(), mapped, static_cast<std::size_t>(byteCount));
      vkUnmapMemory(device, outputMemory);
      return results;
    }

    void destroy(SmokeBuffer& buffer) const {
      if (buffer.buffer) {
        vkDestroyBuffer(device, buffer.buffer, nullptr);
        buffer.buffer = VK_NULL_HANDLE;
      }
      if (buffer.memory) {
        vkFreeMemory(device, buffer.memory, nullptr);
        buffer.memory = VK_NULL_HANDLE;
      }
    }

    void destroy(QueryBuffers& buffers) const {
      destroy(buffers.rays);
      destroy(buffers.output);
      destroy(buffers.counts);
      if (buffers.commandPool) {
        vkDestroyCommandPool(device, buffers.commandPool, nullptr);
        buffers.commandPool = VK_NULL_HANDLE;
      }
      buffers.rayCapacityBytes = 0;
      buffers.outputCapacityBytes = 0;
      buffers.inUse = false;
    }

    VkInstance instance{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    std::uint32_t queueFamily{kInvalidQueueFamily};
    VkQueue queue{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorLayout{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkShaderModule closestShader{VK_NULL_HANDLE};
    VkShaderModule anyShader{VK_NULL_HANDLE};
    VkPipeline closestPipeline{VK_NULL_HANDLE};
    VkPipeline anyPipeline{VK_NULL_HANDLE};
    std::vector<SmokeBuffer> sceneBuffers;
    std::array<std::uint32_t, 4> sceneCounts0{};
    std::array<std::uint32_t, 4> sceneCounts1{};
    std::uint32_t transformCount{0};
    std::uint32_t torusCount{0};
    mutable std::mutex queryBufferMutex;
    mutable std::mutex queueSubmitMutex;
    mutable std::vector<std::unique_ptr<QueryBuffers>> queryBufferPool;
  };
#else
  struct VulkanWavefrontPreparedScene::Private {};
#endif

  bool VulkanWavefrontSmokeKernel::deviceAvailable() const {
    return deviceUnavailableReason().empty();
  }

  std::string VulkanWavefrontSmokeKernel::deviceUnavailableReason() const {
    static const std::string reason = probeDeviceUnavailableReason();
    return reason;
  }

  std::string VulkanWavefrontSmokeKernel::probeDeviceUnavailableReason() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanSmokeRuntime().deviceUnavailableReason();
#else
    return "Vulkan wavefront intersection backend is not enabled in this build";
#endif
  }

  bool VulkanWavefrontSmokeKernel::renderPathAvailable() const {
    return renderPathUnavailableReason().empty();
  }

  std::string VulkanWavefrontSmokeKernel::renderPathUnavailableReason() const {
    static const std::string reason = probeRenderPathUnavailableReason();
    return reason;
  }

  std::string VulkanWavefrontSmokeKernel::probeRenderPathUnavailableReason() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanSmokeRuntime().renderPathUnavailableReason();
#else
    return "Vulkan wavefront intersection backend is not enabled in this build";
#endif
  }

  std::vector<std::uint32_t> VulkanWavefrontSmokeKernel::runDummyHitMissKernel(
    const std::vector<std::uint32_t>& rayIds) const {
    if (rayIds.empty()) {
      return {};
    }

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanSmokeRuntime().runDummyHitMissKernel(rayIds);
#else
    throw std::runtime_error("Vulkan wavefront backend is not enabled");
#endif
  }

  std::vector<GpuIntersectionHitRecord> VulkanWavefrontSmokeKernel::runBasicClosestHitKernel(
    const GpuIntersectionSceneBuffers& scene, const std::vector<GpuIntersectionRay>& rays) const {
    return runTimedBasicClosestHitKernel(scene, rays).hits;
  }

  VulkanWavefrontClosestHitKernelResult VulkanWavefrontSmokeKernel::runTimedBasicClosestHitKernel(
    const GpuIntersectionSceneBuffers& scene, const std::vector<GpuIntersectionRay>& rays) const {
    if (rays.empty()) {
      return {};
    }

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanSmokeRuntime().runTimedBasicClosestHitKernel(scene, rays);
#else
    (void)scene;
    throw std::runtime_error("Vulkan wavefront backend is not enabled");
#endif
  }

  std::vector<GpuIntersectionOcclusionRecord> VulkanWavefrontSmokeKernel::runBasicAnyHitKernel(
    const GpuIntersectionSceneBuffers& scene, const std::vector<GpuIntersectionRay>& rays) const {
    return runTimedBasicAnyHitKernel(scene, rays).records;
  }

  VulkanWavefrontAnyHitKernelResult VulkanWavefrontSmokeKernel::runTimedBasicAnyHitKernel(
    const GpuIntersectionSceneBuffers& scene, const std::vector<GpuIntersectionRay>& rays) const {
    if (rays.empty()) {
      return {};
    }

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanSmokeRuntime().runTimedBasicAnyHitKernel(scene, rays);
#else
    (void)scene;
    throw std::runtime_error("Vulkan wavefront backend is not enabled");
#endif
  }

  VulkanWavefrontPreparedScene::VulkanWavefrontPreparedScene(
    const GpuIntersectionSceneBuffers& scene)
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
      : p(std::make_shared<Private>(scene)) {
#else
      : p(std::make_shared<Private>()) {
    (void)scene;
    throw std::runtime_error("Vulkan wavefront backend is not enabled");
#endif
  }

  VulkanWavefrontPreparedScene::~VulkanWavefrontPreparedScene() = default;

  std::shared_ptr<const VulkanWavefrontPreparedRayBatch>
  VulkanWavefrontPreparedScene::prepareRays(const std::vector<GpuIntersectionRay>& rays) const {
    auto batch =
      std::shared_ptr<VulkanWavefrontPreparedRayBatch>(new VulkanWavefrontPreparedRayBatch);
    if (rays.empty()) {
      return batch;
    }
    if (rays.size() > std::numeric_limits<std::uint32_t>::max()) {
      throw std::runtime_error("Vulkan prepared wavefront ray batch has too many rays");
    }

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    batch->p->sceneLifetime = p;
    batch->p->device = p->device;
    batch->p->rayCount = static_cast<std::uint64_t>(rays.size());
    const VkDeviceSize rayBytes = p->byteCountForRecords<GpuIntersectionRay>(rays.size());
    batch->p->rays = p->createStorageBuffer(rayBytes, rays.data());
    const std::array<std::uint32_t, 12> counts{
      p->sceneCounts0[0], p->sceneCounts0[1], p->sceneCounts0[2],
      p->sceneCounts0[3], p->sceneCounts1[0], p->sceneCounts1[1],
      p->sceneCounts1[2], p->sceneCounts1[3], static_cast<std::uint32_t>(rays.size()),
      p->transformCount,  p->torusCount,      0u,
    };
    batch->p->counts = p->createStorageBuffer(sizeof(counts), counts.data());
    return batch;
#else
    throw std::runtime_error("Vulkan wavefront backend is not enabled");
#endif
  }

  VulkanWavefrontClosestHitKernelResult VulkanWavefrontPreparedScene::runTimedBasicClosestHitKernel(
    const std::vector<GpuIntersectionRay>& rays) const {
    if (rays.empty()) {
      return {};
    }
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return p->runClosest(rays);
#else
    throw std::runtime_error("Vulkan wavefront backend is not enabled");
#endif
  }

  VulkanWavefrontClosestHitKernelResult VulkanWavefrontPreparedScene::runTimedBasicClosestHitKernel(
    const VulkanWavefrontPreparedRayBatch& rays) const {
    if (rays.rayCount() == 0) {
      return {};
    }
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return p->runClosestPrepared(rays.p->rayCount, rays.p->rays, rays.p->counts);
#else
    throw std::runtime_error("Vulkan wavefront backend is not enabled");
#endif
  }

  VulkanWavefrontAnyHitKernelResult VulkanWavefrontPreparedScene::runTimedBasicAnyHitKernel(
    const std::vector<GpuIntersectionRay>& rays) const {
    if (rays.empty()) {
      return {};
    }
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return p->runAny(rays);
#else
    throw std::runtime_error("Vulkan wavefront backend is not enabled");
#endif
  }

  VulkanWavefrontAnyHitKernelResult VulkanWavefrontPreparedScene::runTimedBasicAnyHitKernel(
    const VulkanWavefrontPreparedRayBatch& rays) const {
    if (rays.rayCount() == 0) {
      return {};
    }
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return p->runAnyPrepared(rays.p->rayCount, rays.p->rays, rays.p->counts);
#else
    throw std::runtime_error("Vulkan wavefront backend is not enabled");
#endif
  }
}
