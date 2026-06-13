#include "render/VulkanTracingAccumulationKernel.h"

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
#include "render/VulkanTracingAccumulationAdd.generated.h"
#include "render/VulkanTracingAccumulationClear.generated.h"
#include "render/VulkanTracingAccumulationResolve.generated.h"

#include <vulkan/vulkan.h>
#endif

#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace render {
  namespace {
    std::size_t checkedPixelCount(const TracingAccumulationLayout& layout) {
      const std::uint64_t pixels = layout.pixelCount();
      if (pixels > std::numeric_limits<std::size_t>::max()) {
        throw std::overflow_error("Vulkan tracing accumulation pixel count is too large");
      }
      if (pixels > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("Vulkan tracing accumulation dispatch is too large");
      }
      return static_cast<std::size_t>(pixels);
    }

    void validateSampleFrames(const TracingAccumulationLayout& layout,
                              const std::vector<std::vector<Colord>>& sampleFrames) {
      const std::size_t pixels = checkedPixelCount(layout);
      for (const std::vector<Colord>& frame : sampleFrames) {
        if (frame.size() != pixels) {
          throw std::invalid_argument(
            "Vulkan tracing accumulation sample frame size does not match the layout");
        }
      }
    }

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    struct Rgba32f {
      float r{0.0f};
      float g{0.0f};
      float b{0.0f};
      float a{0.0f};
    };

    Rgba32f toRgba32f(const Colord& color) {
      return {static_cast<float>(color.r()), static_cast<float>(color.g()),
              static_cast<float>(color.b()), 0.0f};
    }

    Colord toColor(const Rgba32f& color) {
      return Colord(color.r, color.g, color.b);
    }

    class VulkanAccumulationRuntime final {
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
          return "Vulkan tracing accumulation found no physical device with a compute queue";
        }
        return "";
      }

      bool accumulationAvailable() const {
        return accumulationUnavailableReason().empty();
      }

      std::string accumulationUnavailableReason() const {
        try {
          VkInstance instance = createInstance();
          InstanceGuard instanceGuard;
          instanceGuard.instance = instance;

          const DeviceSelection selection = selectDevice(instance);
          if (selection.device == VK_NULL_HANDLE) {
            return "Vulkan tracing accumulation found no physical device with a compute queue";
          }

          VkDevice device = createDevice(selection.device, selection.queueFamily);
          DeviceGuard deviceGuard;
          deviceGuard.device = device;

          VkShaderModule clearShader =
            createShaderModule(device, vulkan_shaders::tracingAccumulationClearShaderSpirv.data(),
                               vulkan_shaders::tracingAccumulationClearShaderSpirv.size());
          ShaderGuard clearShaderGuard;
          clearShaderGuard.device = device;
          clearShaderGuard.shaderModule = clearShader;

          VkShaderModule addShader =
            createShaderModule(device, vulkan_shaders::tracingAccumulationAddShaderSpirv.data(),
                               vulkan_shaders::tracingAccumulationAddShaderSpirv.size());
          ShaderGuard addShaderGuard;
          addShaderGuard.device = device;
          addShaderGuard.shaderModule = addShader;

          VkShaderModule resolveShader = createShaderModule(
            device, vulkan_shaders::tracingAccumulationResolveShaderSpirv.data(),
            vulkan_shaders::tracingAccumulationResolveShaderSpirv.size());
          ShaderGuard resolveShaderGuard;
          resolveShaderGuard.device = device;
          resolveShaderGuard.shaderModule = resolveShader;

          VkDescriptorSetLayout clearLayout = createDescriptorLayout(device, 4);
          DescriptorLayoutGuard clearLayoutGuard;
          clearLayoutGuard.device = device;
          clearLayoutGuard.layout = clearLayout;
          VkDescriptorSetLayout addLayout = createDescriptorLayout(device, 5);
          DescriptorLayoutGuard addLayoutGuard;
          addLayoutGuard.device = device;
          addLayoutGuard.layout = addLayout;
          VkDescriptorSetLayout resolveLayout = createDescriptorLayout(device, 4);
          DescriptorLayoutGuard resolveLayoutGuard;
          resolveLayoutGuard.device = device;
          resolveLayoutGuard.layout = resolveLayout;

          VkPipelineLayout clearPipelineLayout = createPipelineLayout(device, clearLayout);
          PipelineLayoutGuard clearPipelineLayoutGuard;
          clearPipelineLayoutGuard.device = device;
          clearPipelineLayoutGuard.layout = clearPipelineLayout;
          VkPipelineLayout addPipelineLayout = createPipelineLayout(device, addLayout);
          PipelineLayoutGuard addPipelineLayoutGuard;
          addPipelineLayoutGuard.device = device;
          addPipelineLayoutGuard.layout = addPipelineLayout;
          VkPipelineLayout resolvePipelineLayout = createPipelineLayout(device, resolveLayout);
          PipelineLayoutGuard resolvePipelineLayoutGuard;
          resolvePipelineLayoutGuard.device = device;
          resolvePipelineLayoutGuard.layout = resolvePipelineLayout;

          VkPipeline clearPipeline = createPipeline(device, clearShader, clearPipelineLayout);
          PipelineGuard clearPipelineGuard;
          clearPipelineGuard.device = device;
          clearPipelineGuard.pipeline = clearPipeline;
          VkPipeline addPipeline = createPipeline(device, addShader, addPipelineLayout);
          PipelineGuard addPipelineGuard;
          addPipelineGuard.device = device;
          addPipelineGuard.pipeline = addPipeline;
          VkPipeline resolvePipeline =
            createPipeline(device, resolveShader, resolvePipelineLayout);
          PipelineGuard resolvePipelineGuard;
          resolvePipelineGuard.device = device;
          resolvePipelineGuard.pipeline = resolvePipeline;
          return "";
        } catch (const std::runtime_error& e) {
          return e.what();
        }
      }

      VulkanTracingAccumulationResult
      runClearAddResolve(const TracingAccumulationLayout& layout,
                         const std::vector<std::vector<Colord>>& sampleFrames) const {
        validateSampleFrames(layout, sampleFrames);
        const std::size_t pixels = checkedPixelCount(layout);
        const bool hasMoment = layout.hasMomentBuffer();

        VkInstance instance = createInstance();
        InstanceGuard instanceGuard;
        instanceGuard.instance = instance;

        const DeviceSelection selection = selectDevice(instance);
        if (selection.device == VK_NULL_HANDLE) {
          throw std::runtime_error("Vulkan tracing accumulation requires a compute device");
        }

        VkDevice device = createDevice(selection.device, selection.queueFamily);
        DeviceGuard deviceGuard;
        deviceGuard.device = device;

        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, selection.queueFamily, 0, &queue);

        BufferVectorGuard buffers;
        buffers.device = device;
        buffers.buffers.push_back(
          createStorageBuffer(device, selection.device, byteCount<Rgba32f>(pixels), nullptr));
        buffers.buffers.push_back(createStorageBuffer(
          device, selection.device, byteCount<std::uint32_t>(pixels), nullptr));
        buffers.buffers.push_back(createStorageBuffer(
          device, selection.device, byteCount<Rgba32f>(hasMoment ? pixels : 1), nullptr));
        buffers.buffers.push_back(createStorageBuffer(
          device, selection.device, byteCount<unsigned int>(pixels), nullptr));

        const std::array<std::uint32_t, 4> params{
          static_cast<std::uint32_t>(pixels), hasMoment ? 1u : 0u, 0u, 0u};
        SmokeBuffer paramsBuffer =
          createStorageBuffer(device, selection.device, sizeof(params), params.data());
        BufferGuard paramsGuard;
        paramsGuard.device = device;
        paramsGuard.buffer = paramsBuffer;

        VkShaderModule clearShader =
          createShaderModule(device, vulkan_shaders::tracingAccumulationClearShaderSpirv.data(),
                             vulkan_shaders::tracingAccumulationClearShaderSpirv.size());
        ShaderGuard clearShaderGuard;
        clearShaderGuard.device = device;
        clearShaderGuard.shaderModule = clearShader;
        VkShaderModule addShader =
          createShaderModule(device, vulkan_shaders::tracingAccumulationAddShaderSpirv.data(),
                             vulkan_shaders::tracingAccumulationAddShaderSpirv.size());
        ShaderGuard addShaderGuard;
        addShaderGuard.device = device;
        addShaderGuard.shaderModule = addShader;
        VkShaderModule resolveShader =
          createShaderModule(device, vulkan_shaders::tracingAccumulationResolveShaderSpirv.data(),
                             vulkan_shaders::tracingAccumulationResolveShaderSpirv.size());
        ShaderGuard resolveShaderGuard;
        resolveShaderGuard.device = device;
        resolveShaderGuard.shaderModule = resolveShader;

        VkDescriptorSetLayout clearLayout = createDescriptorLayout(device, 4);
        DescriptorLayoutGuard clearLayoutGuard;
        clearLayoutGuard.device = device;
        clearLayoutGuard.layout = clearLayout;
        VkDescriptorSetLayout addLayout = createDescriptorLayout(device, 5);
        DescriptorLayoutGuard addLayoutGuard;
        addLayoutGuard.device = device;
        addLayoutGuard.layout = addLayout;
        VkDescriptorSetLayout resolveLayout = createDescriptorLayout(device, 4);
        DescriptorLayoutGuard resolveLayoutGuard;
        resolveLayoutGuard.device = device;
        resolveLayoutGuard.layout = resolveLayout;

        VkPipelineLayout clearPipelineLayout = createPipelineLayout(device, clearLayout);
        PipelineLayoutGuard clearPipelineLayoutGuard;
        clearPipelineLayoutGuard.device = device;
        clearPipelineLayoutGuard.layout = clearPipelineLayout;
        VkPipelineLayout addPipelineLayout = createPipelineLayout(device, addLayout);
        PipelineLayoutGuard addPipelineLayoutGuard;
        addPipelineLayoutGuard.device = device;
        addPipelineLayoutGuard.layout = addPipelineLayout;
        VkPipelineLayout resolvePipelineLayout = createPipelineLayout(device, resolveLayout);
        PipelineLayoutGuard resolvePipelineLayoutGuard;
        resolvePipelineLayoutGuard.device = device;
        resolvePipelineLayoutGuard.layout = resolvePipelineLayout;

        VkPipeline clearPipeline = createPipeline(device, clearShader, clearPipelineLayout);
        PipelineGuard clearPipelineGuard;
        clearPipelineGuard.device = device;
        clearPipelineGuard.pipeline = clearPipeline;
        VkPipeline addPipeline = createPipeline(device, addShader, addPipelineLayout);
        PipelineGuard addPipelineGuard;
        addPipelineGuard.device = device;
        addPipelineGuard.pipeline = addPipeline;
        VkPipeline resolvePipeline = createPipeline(device, resolveShader, resolvePipelineLayout);
        PipelineGuard resolvePipelineGuard;
        resolvePipelineGuard.device = device;
        resolvePipelineGuard.pipeline = resolvePipeline;

        VkCommandPool commandPool = createCommandPool(device, selection.queueFamily);
        CommandPoolGuard commandPoolGuard;
        commandPoolGuard.device = device;
        commandPoolGuard.pool = commandPool;

        dispatch(device, queue, commandPool, clearPipeline, clearPipelineLayout, clearLayout,
                 {
                   {buffers.buffers[0].buffer, buffers.buffers[0].byteCount},
                   {buffers.buffers[1].buffer, buffers.buffers[1].byteCount},
                   {buffers.buffers[2].buffer, buffers.buffers[2].byteCount},
                   {paramsBuffer.buffer, paramsBuffer.byteCount},
                 },
                 pixels);

        for (const std::vector<Colord>& frame : sampleFrames) {
          std::vector<Rgba32f> samples;
          samples.reserve(frame.size());
          for (const Colord& color : frame) {
            samples.push_back(toRgba32f(color));
          }

          SmokeBuffer sampleBuffer =
            createStorageBuffer(device, selection.device, byteCount<Rgba32f>(samples.size()),
                                samples.data());
          BufferGuard sampleGuard;
          sampleGuard.device = device;
          sampleGuard.buffer = sampleBuffer;

          dispatch(device, queue, commandPool, addPipeline, addPipelineLayout, addLayout,
                   {
                     {buffers.buffers[0].buffer, buffers.buffers[0].byteCount},
                     {buffers.buffers[1].buffer, buffers.buffers[1].byteCount},
                     {buffers.buffers[2].buffer, buffers.buffers[2].byteCount},
                     {sampleBuffer.buffer, sampleBuffer.byteCount},
                     {paramsBuffer.buffer, paramsBuffer.byteCount},
                   },
                   pixels);
        }

        dispatch(device, queue, commandPool, resolvePipeline, resolvePipelineLayout, resolveLayout,
                 {
                   {buffers.buffers[0].buffer, buffers.buffers[0].byteCount},
                   {buffers.buffers[1].buffer, buffers.buffers[1].byteCount},
                   {buffers.buffers[3].buffer, buffers.buffers[3].byteCount},
                   {paramsBuffer.buffer, paramsBuffer.byteCount},
                 },
                 pixels);

        VulkanTracingAccumulationResult result;
        result.width = layout.width;
        result.height = layout.height;
        result.diagnostics =
          TracingAccumulationDiagnostics::forLayout(layout, "vulkan", "gpu_device");
        result.diagnostics.recordClear();
        result.diagnostics.recordAdd(static_cast<std::uint64_t>(pixels) *
                                       static_cast<std::uint64_t>(sampleFrames.size()),
                                     static_cast<std::uint64_t>(sampleFrames.size()));
        result.diagnostics.recordResolve();

        const std::vector<Rgba32f> colorSums = readBackRecords<Rgba32f>(
          device, buffers.buffers[0].memory, byteCount<Rgba32f>(pixels), pixels,
          "Vulkan tracing accumulation color-sum buffer mapping");
        result.diagnostics.recordReadback(byteCount<Rgba32f>(pixels));
        result.colorSums.reserve(colorSums.size());
        for (const Rgba32f& color : colorSums) {
          result.colorSums.push_back(toColor(color));
        }

        result.sampleCounts = readBackRecords<std::uint32_t>(
          device, buffers.buffers[1].memory, byteCount<std::uint32_t>(pixels), pixels,
          "Vulkan tracing accumulation sample-count buffer mapping");
        result.diagnostics.recordReadback(byteCount<std::uint32_t>(pixels));

        if (hasMoment) {
          const std::vector<Rgba32f> moments = readBackRecords<Rgba32f>(
            device, buffers.buffers[2].memory, byteCount<Rgba32f>(pixels), pixels,
            "Vulkan tracing accumulation second-moment buffer mapping");
          result.diagnostics.recordReadback(byteCount<Rgba32f>(pixels));
          result.secondMoments.reserve(moments.size());
          for (const Rgba32f& color : moments) {
            result.secondMoments.push_back(toColor(color));
          }
        }

        result.resolved = readBackRecords<unsigned int>(
          device, buffers.buffers[3].memory, byteCount<unsigned int>(pixels), pixels,
          "Vulkan tracing accumulation resolve buffer mapping");
        result.diagnostics.recordReadback(byteCount<unsigned int>(pixels));
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
          if (buffer.buffer) {
            vkDestroyBuffer(device, buffer.buffer, nullptr);
          }
          if (buffer.memory) {
            vkFreeMemory(device, buffer.memory, nullptr);
          }
        }

        VkDevice device{VK_NULL_HANDLE};
        SmokeBuffer buffer;
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

      void check(VkResult result, const char* operation) const {
        if (result != VK_SUCCESS) {
          throw std::runtime_error(std::string(operation) + " failed");
        }
      }

      VkInstance createInstance() const {
        VkApplicationInfo applicationInfo{};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "raytracer Vulkan tracing accumulation";
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pEngineName = "raytracer";
        applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &applicationInfo;

        VkInstance instance = VK_NULL_HANDLE;
        check(vkCreateInstance(&createInfo, nullptr, &instance),
              "Vulkan tracing accumulation instance creation");
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
              "Vulkan tracing accumulation logical device creation");
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
        throw std::runtime_error(
          "Vulkan tracing accumulation requires host-coherent buffer memory");
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
              "Vulkan tracing accumulation buffer creation");

        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, result.buffer, &requirements);

        VkMemoryAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = requirements.size;
        allocateInfo.memoryTypeIndex =
          findHostVisibleMemoryType(physicalDevice, requirements.memoryTypeBits);
        check(vkAllocateMemory(device, &allocateInfo, nullptr, &result.memory),
              "Vulkan tracing accumulation buffer memory allocation");
        check(vkBindBufferMemory(device, result.buffer, result.memory, 0),
              "Vulkan tracing accumulation buffer binding");

        if (initialData) {
          writeBuffer(device, result, byteCount, initialData,
                      "Vulkan tracing accumulation input buffer mapping");
        }
        return result;
      }

      void writeBuffer(VkDevice device, const SmokeBuffer& buffer, VkDeviceSize byteCount,
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
          throw std::runtime_error("Vulkan tracing accumulation buffer is too large");
        }
        return static_cast<VkDeviceSize>(recordCount) * static_cast<VkDeviceSize>(sizeof(Record));
      }

      VkShaderModule createShaderModule(VkDevice device, const std::uint32_t* words,
                                        std::size_t wordCount) const {
        VkShaderModuleCreateInfo shaderInfo{};
        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = wordCount * sizeof(std::uint32_t);
        shaderInfo.pCode = words;

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        check(vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule),
              "Vulkan tracing accumulation shader module creation");
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
        check(vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr,
                                          &descriptorLayout),
              "Vulkan tracing accumulation descriptor layout creation");
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
              "Vulkan tracing accumulation pipeline layout creation");
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
        check(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                       &pipeline),
              "Vulkan tracing accumulation compute pipeline creation");
        return pipeline;
      }

      VkDescriptorPool createDescriptorPool(VkDevice device,
                                            std::uint32_t descriptorCount) const {
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
              "Vulkan tracing accumulation descriptor pool creation");
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
              "Vulkan tracing accumulation descriptor set allocation");
        return descriptorSet;
      }

      void updateDescriptorSet(
        VkDevice device, VkDescriptorSet descriptorSet,
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

      VkCommandPool createCommandPool(VkDevice device, std::uint32_t queueFamily) const {
        VkCommandPoolCreateInfo commandPoolInfo{};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.queueFamilyIndex = queueFamily;

        VkCommandPool commandPool = VK_NULL_HANDLE;
        check(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool),
              "Vulkan tracing accumulation command pool creation");
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
              "Vulkan tracing accumulation command buffer allocation");
        return commandBuffer;
      }

      void dispatch(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkPipeline pipeline,
                    VkPipelineLayout pipelineLayout, VkDescriptorSetLayout descriptorLayout,
                    const std::vector<std::pair<VkBuffer, VkDeviceSize>>& descriptors,
                    std::size_t pixelCount) const {
        DescriptorPoolGuard descriptorPool;
        descriptorPool.device = device;
        descriptorPool.pool =
          createDescriptorPool(device, static_cast<std::uint32_t>(descriptors.size()));
        VkDescriptorSet descriptorSet =
          allocateDescriptorSet(device, descriptorPool.pool, descriptorLayout);
        updateDescriptorSet(device, descriptorSet, descriptors);

        check(vkResetCommandPool(device, commandPool, 0),
              "Vulkan tracing accumulation command pool reset");
        VkCommandBuffer commandBuffer = allocateCommandBuffer(device, commandPool);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        check(vkBeginCommandBuffer(commandBuffer, &beginInfo),
              "Vulkan tracing accumulation command buffer begin");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0,
                                1, &descriptorSet, 0, nullptr);
        vkCmdDispatch(commandBuffer, static_cast<std::uint32_t>((pixelCount + 63) / 64), 1, 1);
        check(vkEndCommandBuffer(commandBuffer),
              "Vulkan tracing accumulation command buffer end");

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        check(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE),
              "Vulkan tracing accumulation queue submit");
        check(vkQueueWaitIdle(queue), "Vulkan tracing accumulation queue wait");
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

  std::size_t VulkanTracingAccumulationResult::pixelIndex(int x, int y) const {
    if (x < 0 || y < 0 || x >= width || y >= height) {
      throw std::out_of_range("Vulkan tracing accumulation result pixel is out of range");
    }
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
           static_cast<std::size_t>(x);
  }

  Colord VulkanTracingAccumulationResult::colorSumAt(int x, int y) const {
    return colorSums.at(pixelIndex(x, y));
  }

  std::uint32_t VulkanTracingAccumulationResult::sampleCountAt(int x, int y) const {
    return sampleCounts.at(pixelIndex(x, y));
  }

  Colord VulkanTracingAccumulationResult::secondMomentAt(int x, int y) const {
    return secondMoments.at(pixelIndex(x, y));
  }

  unsigned int VulkanTracingAccumulationResult::resolvedAt(int x, int y) const {
    return resolved.at(pixelIndex(x, y));
  }

  bool VulkanTracingAccumulationKernel::deviceAvailable() const {
    return deviceUnavailableReason().empty();
  }

  std::string VulkanTracingAccumulationKernel::deviceUnavailableReason() const {
    static const std::string reason = probeDeviceUnavailableReason();
    return reason;
  }

  std::string VulkanTracingAccumulationKernel::probeDeviceUnavailableReason() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanAccumulationRuntime().deviceUnavailableReason();
#else
    return "Vulkan tracing accumulation kernels are not enabled in this build";
#endif
  }

  bool VulkanTracingAccumulationKernel::accumulationAvailable() const {
    return accumulationUnavailableReason().empty();
  }

  std::string VulkanTracingAccumulationKernel::accumulationUnavailableReason() const {
    static const std::string reason = probeAccumulationUnavailableReason();
    return reason;
  }

  std::string VulkanTracingAccumulationKernel::probeAccumulationUnavailableReason() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanAccumulationRuntime().accumulationUnavailableReason();
#else
    return "Vulkan tracing accumulation kernels are not enabled in this build";
#endif
  }

  VulkanTracingAccumulationResult VulkanTracingAccumulationKernel::runClearAddResolve(
    const TracingAccumulationLayout& layout,
    const std::vector<std::vector<Colord>>& sampleFrames) const {
    layout.validate();
    validateSampleFrames(layout, sampleFrames);
    if (sampleFrames.size() > std::numeric_limits<std::uint32_t>::max()) {
      throw std::runtime_error("Vulkan tracing accumulation sample count is too large");
    }
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanAccumulationRuntime().runClearAddResolve(layout, sampleFrames);
#else
    throw std::runtime_error("Vulkan tracing accumulation kernels are not enabled");
#endif
  }
}
