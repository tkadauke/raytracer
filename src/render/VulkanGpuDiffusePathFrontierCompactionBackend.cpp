#include "render/VulkanGpuDiffusePathFrontierCompactionBackend.h"

#include "TimingHelpers.h"

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
#include "render/VulkanDiffusePathFrontierCompaction.generated.h"

#include <vulkan/vulkan.h>
#endif

#include <chrono>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace render {
  namespace {
    static_assert(sizeof(GpuIntersectionRay) == 64);
    static_assert(alignof(GpuIntersectionRay) == 16);
    static_assert(sizeof(GpuDiffusePathStateRecord) == 160);
    static_assert(alignof(GpuDiffusePathStateRecord) == 16);

    void validateRetainedPathIndices(std::size_t inputPathCount,
                                     const std::vector<std::uint32_t>& retainedPathIndices) {
      std::uint32_t previous = 0;
      bool hasPrevious = false;
      for (const std::uint32_t index : retainedPathIndices) {
        if (index >= inputPathCount) {
          throw std::out_of_range(
            "Vulkan diffuse frontier compaction retained path index is out of range");
        }
        if (hasPrevious && index <= previous) {
          throw std::invalid_argument(
            "Vulkan diffuse frontier compaction retained path indices must be strictly increasing");
        }
        previous = index;
        hasPrevious = true;
      }
    }

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    void validateDispatchCount(std::size_t retainedPathCount) {
      if (retainedPathCount > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error(
          "Vulkan diffuse frontier compaction retained path count exceeds shader index range");
      }
    }

    class VulkanDiffuseFrontierCompactionRuntime final {
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
          return "Vulkan diffuse frontier compaction found no physical device with a compute queue";
        }
        return "";
      }

      bool compactionPathAvailable() const {
        return compactionPathUnavailableReason().empty();
      }

      std::string compactionPathUnavailableReason() const {
        try {
          VkInstance instance = createInstance();
          InstanceGuard instanceGuard;
          instanceGuard.instance = instance;

          const DeviceSelection selection = selectDevice(instance);
          if (selection.device == VK_NULL_HANDLE) {
            return "Vulkan diffuse frontier compaction found no physical device with a compute "
                   "queue";
          }

          VkDevice device = createDevice(selection.device, selection.queueFamily);
          DeviceGuard deviceGuard;
          deviceGuard.device = device;

          VkShaderModule shader = createShaderModule(
            device, vulkan_shaders::diffusePathFrontierCompactionShaderSpirv.data(),
            vulkan_shaders::diffusePathFrontierCompactionShaderSpirv.size());
          ShaderGuard shaderGuard;
          shaderGuard.device = device;
          shaderGuard.shaderModule = shader;

          VkDescriptorSetLayout descriptorLayout = createDescriptorLayout(device, 3);
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
          return "";
        } catch (const std::runtime_error& e) {
          return e.what();
        }
      }

      GpuDiffusePathFrontierCompactionResult
      compact(const std::vector<GpuDiffusePathStateRecord>& sourceRecords,
              const std::vector<std::uint32_t>& retainedPathIndices) const {
        GpuDiffusePathFrontierCompactionResult result;
        result.executionPath = "vulkan_diffuse_frontier_compaction";
        result.pathStateResidency = "vulkan_host_visible_diffuse_path_state";
        result.inputPathCount = sourceRecords.size();
        result.retainedPathIndices = retainedPathIndices;

        if (retainedPathIndices.empty()) {
          return result;
        }
        validateDispatchCount(retainedPathIndices.size());

        const auto uploadStart = std::chrono::steady_clock::now();
        VkInstance instance = createInstance();
        InstanceGuard instanceGuard;
        instanceGuard.instance = instance;

        const DeviceSelection selection = selectDevice(instance);
        if (selection.device == VK_NULL_HANDLE) {
          throw std::runtime_error("Vulkan diffuse frontier compaction requires a compute device");
        }

        VkDevice device = createDevice(selection.device, selection.queueFamily);
        DeviceGuard deviceGuard;
        deviceGuard.device = device;

        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, selection.queueFamily, 0, &queue);

        BufferVectorGuard buffers;
        buffers.device = device;
        buffers.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, sourceRecords));
        buffers.buffers.push_back(
          createStorageBufferFromVector(device, selection.device, retainedPathIndices));
        buffers.buffers.push_back(createStorageBuffer(
          device, selection.device,
          byteCount<GpuDiffusePathStateRecord>(retainedPathIndices.size()), nullptr));

        VkShaderModule shader = createShaderModule(
          device, vulkan_shaders::diffusePathFrontierCompactionShaderSpirv.data(),
          vulkan_shaders::diffusePathFrontierCompactionShaderSpirv.size());
        ShaderGuard shaderGuard;
        shaderGuard.device = device;
        shaderGuard.shaderModule = shader;

        VkDescriptorSetLayout descriptorLayout = createDescriptorLayout(device, 3);
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

        VkDescriptorPool descriptorPool = createDescriptorPool(device, 3);
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
        recordDispatch(commandBuffer, pipeline, pipelineLayout, descriptorSet,
                       static_cast<std::uint32_t>(retainedPathIndices.size()));
        const auto uploadEnd = std::chrono::steady_clock::now();

        const auto kernelStart = std::chrono::steady_clock::now();
        submitAndWait(queue, commandBuffer);
        const auto kernelEnd = std::chrono::steady_clock::now();

        const auto readbackStart = std::chrono::steady_clock::now();
        result.retainedRecords = readBackRecords<GpuDiffusePathStateRecord>(
          device, buffers.buffers[2].memory,
          byteCount<GpuDiffusePathStateRecord>(retainedPathIndices.size()),
          retainedPathIndices.size(), "Vulkan diffuse frontier compaction output buffer mapping");
        const auto readbackEnd = std::chrono::steady_clock::now();

        result.uploadWorkerSeconds = detail::secondsBetween(uploadStart, uploadEnd);
        result.kernelWorkerSeconds = detail::secondsBetween(kernelStart, kernelEnd);
        result.readbackWorkerSeconds = detail::secondsBetween(readbackStart, readbackEnd);
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
        applicationInfo.pApplicationName = "raytracer Vulkan diffuse frontier compaction";
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pEngineName = "raytracer";
        applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &applicationInfo;

        VkInstance instance = VK_NULL_HANDLE;
        check(vkCreateInstance(&createInfo, nullptr, &instance),
              "Vulkan diffuse frontier compaction instance creation");
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
              "Vulkan diffuse frontier compaction logical device creation");
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
          "Vulkan diffuse frontier compaction requires host-coherent buffer memory");
      }

      StorageBuffer createStorageBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                                        VkDeviceSize byteCount, const void* initialData) const {
        StorageBuffer result;
        result.byteCount = byteCount;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = byteCount;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device, &bufferInfo, nullptr, &result.buffer),
              "Vulkan diffuse frontier compaction buffer creation");

        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, result.buffer, &requirements);

        VkMemoryAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = requirements.size;
        allocateInfo.memoryTypeIndex =
          findHostVisibleMemoryType(physicalDevice, requirements.memoryTypeBits);
        check(vkAllocateMemory(device, &allocateInfo, nullptr, &result.memory),
              "Vulkan diffuse frontier compaction buffer memory allocation");
        check(vkBindBufferMemory(device, result.buffer, result.memory, 0),
              "Vulkan diffuse frontier compaction buffer binding");

        if (initialData) {
          writeBuffer(device, result, byteCount, initialData,
                      "Vulkan diffuse frontier compaction input buffer mapping");
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
          throw std::runtime_error("Vulkan diffuse frontier compaction buffer is too large");
        }
        return static_cast<VkDeviceSize>(recordCount) * static_cast<VkDeviceSize>(sizeof(Record));
      }

      template<typename Record>
      StorageBuffer createStorageBufferFromVector(VkDevice device, VkPhysicalDevice physicalDevice,
                                                  const std::vector<Record>& records) const {
        if (records.empty()) {
          return createStorageBuffer(device, physicalDevice,
                                     static_cast<VkDeviceSize>(sizeof(Record)), nullptr);
        }
        return createStorageBuffer(device, physicalDevice, byteCount<Record>(records.size()),
                                   records.data());
      }

      VkShaderModule createShaderModule(VkDevice device, const std::uint32_t* words,
                                        std::size_t wordCount) const {
        VkShaderModuleCreateInfo shaderInfo{};
        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = wordCount * sizeof(std::uint32_t);
        shaderInfo.pCode = words;

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        check(vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule),
              "Vulkan diffuse frontier compaction shader module creation");
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
          "Vulkan diffuse frontier compaction descriptor layout creation");
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
              "Vulkan diffuse frontier compaction pipeline layout creation");
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
          "Vulkan diffuse frontier compaction compute pipeline creation");
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
              "Vulkan diffuse frontier compaction descriptor pool creation");
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
              "Vulkan diffuse frontier compaction descriptor set allocation");
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
              "Vulkan diffuse frontier compaction command pool creation");
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
              "Vulkan diffuse frontier compaction command buffer allocation");
        return commandBuffer;
      }

      void recordDispatch(VkCommandBuffer commandBuffer, VkPipeline pipeline,
                          VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                          std::uint32_t retainedPathCount) const {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        check(vkBeginCommandBuffer(commandBuffer, &beginInfo),
              "Vulkan diffuse frontier compaction command buffer begin");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1,
                                &descriptorSet, 0, nullptr);
        vkCmdDispatch(commandBuffer, retainedPathCount, 1, 1);
        check(vkEndCommandBuffer(commandBuffer),
              "Vulkan diffuse frontier compaction command buffer end");
      }

      void submitAndWait(VkQueue queue, VkCommandBuffer commandBuffer) const {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        check(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE),
              "Vulkan diffuse frontier compaction queue submit");
        check(vkQueueWaitIdle(queue), "Vulkan diffuse frontier compaction queue wait");
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

  bool VulkanGpuDiffusePathFrontierCompactionBackend::deviceAvailable() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanDiffuseFrontierCompactionRuntime().deviceAvailable();
#else
    return false;
#endif
  }

  std::string VulkanGpuDiffusePathFrontierCompactionBackend::deviceUnavailableReason() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanDiffuseFrontierCompactionRuntime().deviceUnavailableReason();
#else
    return "Vulkan wavefront support is not enabled in this build";
#endif
  }

  bool VulkanGpuDiffusePathFrontierCompactionBackend::compactionPathAvailable() const {
    return compactionPathUnavailableReason().empty();
  }

  std::string
  VulkanGpuDiffusePathFrontierCompactionBackend::compactionPathUnavailableReason() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanDiffuseFrontierCompactionRuntime().compactionPathUnavailableReason();
#else
    return "Vulkan wavefront support is not enabled in this build";
#endif
  }

  const char* VulkanGpuDiffusePathFrontierCompactionBackend::name() const {
    return "vulkan_diffuse_frontier_compaction";
  }

  const char* VulkanGpuDiffusePathFrontierCompactionBackend::pathStateResidency() const {
    return "vulkan_host_visible_diffuse_path_state";
  }

  GpuDiffusePathFrontierCompactionResult VulkanGpuDiffusePathFrontierCompactionBackend::compact(
    const std::vector<GpuDiffusePathStateRecord>& sourceRecords,
    const std::vector<std::uint32_t>& retainedPathIndices) const {
    validateRetainedPathIndices(sourceRecords.size(), retainedPathIndices);

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanDiffuseFrontierCompactionRuntime().compact(sourceRecords, retainedPathIndices);
#else
    GpuDiffusePathFrontierCompactionResult result;
    result.executionPath = name();
    result.pathStateResidency = pathStateResidency();
    result.inputPathCount = sourceRecords.size();
    result.retainedPathIndices = retainedPathIndices;
    if (retainedPathIndices.empty()) {
      return result;
    }
    throw std::runtime_error(compactionPathUnavailableReason());
#endif
  }
}
