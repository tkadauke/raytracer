#pragma once

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)

#include <vulkan/vulkan.h>

#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace render::detail {

class VulkanComputeHelper {
protected:
  struct DeviceSelection {
    VkPhysicalDevice device{VK_NULL_HANDLE};
    std::uint32_t queueFamily{std::numeric_limits<std::uint32_t>::max()};
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

  template<typename Buffer>
  struct BufferVectorGuard {
    ~BufferVectorGuard() {
      for (Buffer& buffer : buffers) {
        if (buffer.buffer) {
          vkDestroyBuffer(device, buffer.buffer, nullptr);
        }
        if (buffer.memory) {
          vkFreeMemory(device, buffer.memory, nullptr);
        }
      }
    }

    VkDevice device{VK_NULL_HANDLE};
    std::vector<Buffer> buffers;
  };

  static constexpr std::uint32_t kInvalidQueueFamily = std::numeric_limits<std::uint32_t>::max();

  explicit VulkanComputeHelper(const char* context, const char* applicationName)
    : m_context(context), m_applicationName(applicationName) {}

  void check(VkResult result, const char* operation) const {
    if (result != VK_SUCCESS) {
      throw std::runtime_error(std::string(operation) + " failed");
    }
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

  VkInstance createInstance() const {
    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = m_applicationName;
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.pEngineName = "raytracer";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &applicationInfo;

    VkInstance instance = VK_NULL_HANDLE;
    check(vkCreateInstance(&createInfo, nullptr, &instance), contextOp("instance creation").c_str());
    return instance;
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
          contextOp("logical device creation").c_str());
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
      std::string(m_context) + " requires host-coherent buffer memory");
  }

  void writeBuffer(VkDevice device, VkDeviceMemory memory, VkDeviceSize byteCount,
                   const void* data, const char* operation) const {
    void* mapped = nullptr;
    check(vkMapMemory(device, memory, 0, byteCount, 0, &mapped), operation);
    std::memcpy(mapped, data, static_cast<std::size_t>(byteCount));
    vkUnmapMemory(device, memory);
  }

  template<typename Record>
  static VkDeviceSize byteCount(std::size_t recordCount) {
    if (recordCount >
        std::numeric_limits<VkDeviceSize>::max() / static_cast<VkDeviceSize>(sizeof(Record))) {
      throw std::runtime_error("Vulkan compute buffer is too large");
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
          contextOp("shader module creation").c_str());
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
    check(vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr, &descriptorLayout),
          contextOp("descriptor layout creation").c_str());
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
          contextOp("pipeline layout creation").c_str());
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
    check(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline),
          contextOp("compute pipeline creation").c_str());
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
          contextOp("descriptor pool creation").c_str());
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
          contextOp("descriptor set allocation").c_str());
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
          contextOp("command pool creation").c_str());
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
          contextOp("command buffer allocation").c_str());
    return commandBuffer;
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

private:
  const char* m_context;
  const char* m_applicationName;

  std::string contextOp(const char* operation) const {
    return std::string(m_context) + " " + operation;
  }
};

} // namespace render::detail

#endif // RAYTRACER_ENABLE_VULKAN_WAVEFRONT
