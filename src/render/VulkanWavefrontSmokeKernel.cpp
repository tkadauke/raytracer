#include "render/VulkanWavefrontSmokeKernel.h"

#include "render/GpuIntersectionScene.h"

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
        VkInstance instance = VK_NULL_HANDLE;
        try {
          instance = createInstance();
        } catch (const std::runtime_error&) {
          return false;
        }

        InstanceGuard guard;
        guard.instance = instance;

        const DeviceSelection selection = selectDevice(instance);
        return selection.device != VK_NULL_HANDLE;
      }

      bool renderPathAvailable() const {
        try {
          VkInstance instance = createInstance();
          InstanceGuard instanceGuard;
          instanceGuard.instance = instance;

          const DeviceSelection selection = selectDevice(instance);
          if (selection.device == VK_NULL_HANDLE) {
            return false;
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

          VkDescriptorSetLayout descriptorLayout = createDescriptorLayout(device, 6);
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
          return true;
        } catch (const std::runtime_error&) {
          return false;
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
      runTimedTriangleClosestHitKernel(const GpuIntersectionSceneBuffers& scene,
                                       const std::vector<GpuIntersectionRay>& rays) const {
        if (rays.empty()) {
          return {};
        }
        if (!scene.triangleClosestHitKernelEligible()) {
          throw std::invalid_argument(
            "Vulkan triangle closest-hit kernel requires an untransformed triangle scene");
        }
        if (rays.size() > std::numeric_limits<std::uint32_t>::max()) {
          throw std::runtime_error("Vulkan triangle closest-hit ray batch is too large");
        }

        VulkanWavefrontClosestHitKernelResult result;
        const auto uploadStart = std::chrono::steady_clock::now();

        VkInstance instance = createInstance();
        InstanceGuard instanceGuard;
        instanceGuard.instance = instance;

        const DeviceSelection selection = selectDevice(instance);
        if (selection.device == VK_NULL_HANDLE) {
          throw std::runtime_error("Vulkan triangle closest-hit kernel requires a compute device");
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
              "Vulkan triangle closest-hit logical device creation");
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
          createStorageBufferFromVector(device, selection.device, rays));

        const VkDeviceSize hitByteCount =
          byteCountForRecords<GpuIntersectionHitRecord>(rays.size());
        bufferGuard.buffers.push_back(
          createStorageBuffer(device, selection.device, hitByteCount, nullptr));

        const std::array<std::uint32_t, 4> counts{
          static_cast<std::uint32_t>(scene.bvh.size()),
          static_cast<std::uint32_t>(scene.primitives.size()),
          static_cast<std::uint32_t>(scene.triangles.size()),
          static_cast<std::uint32_t>(rays.size()),
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
          device, bufferGuard.buffers[4].memory, hitByteCount, rays.size(),
          "Vulkan triangle closest-hit output buffer mapping");
        const auto readbackEnd = std::chrono::steady_clock::now();

        result.timing.uploadSeconds = secondsBetween(uploadStart, uploadEnd);
        result.timing.kernelSeconds = secondsBetween(kernelStart, kernelEnd);
        result.timing.readbackSeconds = secondsBetween(readbackStart, readbackEnd);
        result.timing.recordExecutionPath("vulkan");
        return result;
      }

      VulkanWavefrontAnyHitKernelResult
      runTimedTriangleAnyHitKernel(const GpuIntersectionSceneBuffers& scene,
                                   const std::vector<GpuIntersectionRay>& rays) const {
        if (rays.empty()) {
          return {};
        }
        if (!scene.triangleClosestHitKernelEligible()) {
          throw std::invalid_argument(
            "Vulkan triangle any-hit kernel requires an untransformed triangle scene");
        }
        if (rays.size() > std::numeric_limits<std::uint32_t>::max()) {
          throw std::runtime_error("Vulkan triangle any-hit ray batch is too large");
        }

        VulkanWavefrontAnyHitKernelResult result;
        const auto uploadStart = std::chrono::steady_clock::now();

        VkInstance instance = createInstance();
        InstanceGuard instanceGuard;
        instanceGuard.instance = instance;

        const DeviceSelection selection = selectDevice(instance);
        if (selection.device == VK_NULL_HANDLE) {
          throw std::runtime_error("Vulkan triangle any-hit kernel requires a compute device");
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
              "Vulkan triangle any-hit logical device creation");
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
          createStorageBufferFromVector(device, selection.device, rays));

        const VkDeviceSize recordByteCount =
          byteCountForRecords<GpuIntersectionOcclusionRecord>(rays.size());
        bufferGuard.buffers.push_back(
          createStorageBuffer(device, selection.device, recordByteCount, nullptr));

        const std::array<std::uint32_t, 4> counts{
          static_cast<std::uint32_t>(scene.bvh.size()),
          static_cast<std::uint32_t>(scene.primitives.size()),
          static_cast<std::uint32_t>(scene.triangles.size()),
          static_cast<std::uint32_t>(rays.size()),
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
          device, bufferGuard.buffers[4].memory, recordByteCount, rays.size(),
          "Vulkan triangle any-hit output buffer mapping");
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

  bool VulkanWavefrontSmokeKernel::deviceAvailable() const {
    static const bool available = probeDeviceAvailable();
    return available;
  }

  bool VulkanWavefrontSmokeKernel::probeDeviceAvailable() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanSmokeRuntime().deviceAvailable();
#else
    return false;
#endif
  }

  bool VulkanWavefrontSmokeKernel::renderPathAvailable() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    static const bool available = VulkanSmokeRuntime().renderPathAvailable();
    return available;
#else
    return false;
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

  std::vector<GpuIntersectionHitRecord> VulkanWavefrontSmokeKernel::runTriangleClosestHitKernel(
    const GpuIntersectionSceneBuffers& scene, const std::vector<GpuIntersectionRay>& rays) const {
    return runTimedTriangleClosestHitKernel(scene, rays).hits;
  }

  VulkanWavefrontClosestHitKernelResult
  VulkanWavefrontSmokeKernel::runTimedTriangleClosestHitKernel(
    const GpuIntersectionSceneBuffers& scene, const std::vector<GpuIntersectionRay>& rays) const {
    if (rays.empty()) {
      return {};
    }

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanSmokeRuntime().runTimedTriangleClosestHitKernel(scene, rays);
#else
    (void)scene;
    throw std::runtime_error("Vulkan wavefront backend is not enabled");
#endif
  }

  std::vector<GpuIntersectionOcclusionRecord> VulkanWavefrontSmokeKernel::runTriangleAnyHitKernel(
    const GpuIntersectionSceneBuffers& scene, const std::vector<GpuIntersectionRay>& rays) const {
    return runTimedTriangleAnyHitKernel(scene, rays).records;
  }

  VulkanWavefrontAnyHitKernelResult VulkanWavefrontSmokeKernel::runTimedTriangleAnyHitKernel(
    const GpuIntersectionSceneBuffers& scene, const std::vector<GpuIntersectionRay>& rays) const {
    if (rays.empty()) {
      return {};
    }

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanSmokeRuntime().runTimedTriangleAnyHitKernel(scene, rays);
#else
    (void)scene;
    throw std::runtime_error("Vulkan wavefront backend is not enabled");
#endif
  }
}
