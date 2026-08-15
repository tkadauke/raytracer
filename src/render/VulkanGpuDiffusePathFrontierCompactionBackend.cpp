#include "render/VulkanGpuDiffusePathFrontierCompactionBackend.h"

#include "render/TimingHelpers.h"

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
#include "render/VulkanDiffusePathFrontierCompaction.generated.h"

#include "VulkanComputeHelper.h"

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

    class VulkanDiffuseFrontierCompactionRuntime final
      : public render::detail::VulkanComputeHelper {
    public:
      VulkanDiffuseFrontierCompactionRuntime()
        : VulkanComputeHelper("Vulkan diffuse frontier compaction",
                              "raytracer Vulkan diffuse frontier compaction") {}
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

        BufferVectorGuard<StorageBuffer> buffers;
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
      struct StorageBuffer {
        VkBuffer buffer{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkDeviceSize byteCount{0};
      };

      StorageBuffer createStorageBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                                        VkDeviceSize size, const void* initialData) const {
        StorageBuffer result;
        result.byteCount = size;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
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
          writeBuffer(device, result.memory, size, initialData,
                      "Vulkan diffuse frontier compaction input buffer mapping");
        }
        return result;
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
