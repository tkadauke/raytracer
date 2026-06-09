#include "render/VulkanWavefrontSmokeKernel.h"

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
#include <vulkan/vulkan.h>
#endif

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace render {
  namespace {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    constexpr std::uint32_t kDummyXorMask = 0xa5a5a5a5u;
    constexpr std::uint32_t kInvalidQueueFamily = std::numeric_limits<std::uint32_t>::max();

    constexpr std::uint32_t spvInstruction(std::uint16_t wordCount, std::uint16_t opcode) {
      return (static_cast<std::uint32_t>(wordCount) << 16u) | opcode;
    }

    const std::vector<std::uint32_t>& dummyComputeShaderSpirv() {
      // clang-format off
      static const std::vector<std::uint32_t> shader{
        0x07230203u, 0x00010000u, 0u, 24u, 0u,
        spvInstruction(2, 17), 1u,
        spvInstruction(3, 14), 0u, 1u,
        spvInstruction(6, 15), 5u, 1u, 0x6e69616du, 0u, 7u,
        spvInstruction(6, 16), 1u, 17u, 1u, 1u, 1u,
        spvInstruction(4, 71), 7u, 11u, 28u,
        spvInstruction(4, 71), 9u, 6u, 4u,
        spvInstruction(5, 72), 10u, 0u, 35u, 0u,
        spvInstruction(3, 71), 10u, 3u,
        spvInstruction(4, 71), 12u, 34u, 0u,
        spvInstruction(4, 71), 12u, 33u, 0u,
        spvInstruction(4, 71), 13u, 34u, 0u,
        spvInstruction(4, 71), 13u, 33u, 1u,
        spvInstruction(2, 19), 2u,
        spvInstruction(3, 33), 3u, 2u,
        spvInstruction(4, 21), 4u, 32u, 0u,
        spvInstruction(4, 23), 5u, 4u, 3u,
        spvInstruction(4, 32), 6u, 1u, 5u,
        spvInstruction(4, 59), 6u, 7u, 1u,
        spvInstruction(4, 43), 4u, 8u, 0u,
        spvInstruction(3, 29), 9u, 4u,
        spvInstruction(3, 30), 10u, 9u,
        spvInstruction(4, 32), 11u, 2u, 10u,
        spvInstruction(4, 59), 11u, 12u, 2u,
        spvInstruction(4, 59), 11u, 13u, 2u,
        spvInstruction(4, 32), 14u, 1u, 4u,
        spvInstruction(4, 32), 15u, 2u, 4u,
        spvInstruction(4, 43), 4u, 16u, kDummyXorMask,
        spvInstruction(5, 54), 2u, 1u, 0u, 3u,
        spvInstruction(2, 248), 17u,
        spvInstruction(5, 65), 14u, 18u, 7u, 8u,
        spvInstruction(4, 61), 4u, 19u, 18u,
        spvInstruction(6, 65), 15u, 20u, 12u, 8u, 19u,
        spvInstruction(4, 61), 4u, 21u, 20u,
        spvInstruction(5, 198), 4u, 22u, 21u, 16u,
        spvInstruction(6, 65), 15u, 23u, 13u, 8u, 19u,
        spvInstruction(3, 62), 23u, 22u,
        spvInstruction(1, 253),
        spvInstruction(1, 56),
      };
      // clang-format on
      return shader;
    }

    void checkVk(VkResult result, const char* operation) {
      if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(operation) + " failed");
      }
    }

    VkInstance createSmokeInstance() {
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
      checkVk(vkCreateInstance(&createInfo, nullptr, &instance),
              "Vulkan wavefront smoke instance creation");
      return instance;
    }

    std::uint32_t computeQueueFamily(VkPhysicalDevice device) {
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

    struct VulkanSmokeDeviceSelection {
      VkPhysicalDevice device{VK_NULL_HANDLE};
      std::uint32_t queueFamily{kInvalidQueueFamily};
    };

    VulkanSmokeDeviceSelection selectSmokeDevice(VkInstance instance) {
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
                                            std::uint32_t memoryTypeBits) {
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

    struct SmokeBuffer {
      VkBuffer buffer{VK_NULL_HANDLE};
      VkDeviceMemory memory{VK_NULL_HANDLE};
    };
#endif
  }

  bool VulkanWavefrontSmokeKernel::deviceAvailable() const {
    static const bool available = probeDeviceAvailable();
    return available;
  }

  bool VulkanWavefrontSmokeKernel::probeDeviceAvailable() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    VkInstance instance = VK_NULL_HANDLE;
    try {
      instance = createSmokeInstance();
    } catch (const std::runtime_error&) {
      return false;
    }

    struct InstanceGuard {
      ~InstanceGuard() {
        if (instance) {
          vkDestroyInstance(instance, nullptr);
        }
      }

      VkInstance instance{VK_NULL_HANDLE};
    } guard;
    guard.instance = instance;

    const VulkanSmokeDeviceSelection selection = selectSmokeDevice(instance);
    return selection.device != VK_NULL_HANDLE;
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
    if (rayIds.size() > std::numeric_limits<std::uint32_t>::max()) {
      throw std::runtime_error("Vulkan wavefront smoke kernel ray batch is too large");
    }

    VkInstance instance = createSmokeInstance();
    struct InstanceGuard {
      ~InstanceGuard() {
        if (instance) {
          vkDestroyInstance(instance, nullptr);
        }
      }

      VkInstance instance{VK_NULL_HANDLE};
    } instanceGuard;
    instanceGuard.instance = instance;

    const VulkanSmokeDeviceSelection selection = selectSmokeDevice(instance);
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
    checkVk(vkCreateDevice(selection.device, &deviceCreateInfo, nullptr, &device),
            "Vulkan wavefront smoke logical device creation");
    struct DeviceGuard {
      ~DeviceGuard() {
        if (device) {
          vkDestroyDevice(device, nullptr);
        }
      }

      VkDevice device{VK_NULL_HANDLE};
    } deviceGuard;
    deviceGuard.device = device;

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, selection.queueFamily, 0, &queue);

    const VkDeviceSize byteCount = static_cast<VkDeviceSize>(rayIds.size() * sizeof(std::uint32_t));
    const auto createStorageBuffer = [&](const void* initialData) {
      SmokeBuffer result;

      VkBufferCreateInfo bufferInfo{};
      bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      bufferInfo.size = byteCount;
      bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      checkVk(vkCreateBuffer(device, &bufferInfo, nullptr, &result.buffer),
              "Vulkan wavefront smoke buffer creation");

      VkMemoryRequirements requirements{};
      vkGetBufferMemoryRequirements(device, result.buffer, &requirements);

      VkMemoryAllocateInfo allocateInfo{};
      allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocateInfo.allocationSize = requirements.size;
      allocateInfo.memoryTypeIndex =
        findHostVisibleMemoryType(selection.device, requirements.memoryTypeBits);
      checkVk(vkAllocateMemory(device, &allocateInfo, nullptr, &result.memory),
              "Vulkan wavefront smoke buffer memory allocation");
      checkVk(vkBindBufferMemory(device, result.buffer, result.memory, 0),
              "Vulkan wavefront smoke buffer binding");

      if (initialData) {
        void* mapped = nullptr;
        checkVk(vkMapMemory(device, result.memory, 0, byteCount, 0, &mapped),
                "Vulkan wavefront smoke input buffer mapping");
        std::memcpy(mapped, initialData, static_cast<std::size_t>(byteCount));
        vkUnmapMemory(device, result.memory);
      }
      return result;
    };

    SmokeBuffer inputBuffer = createStorageBuffer(rayIds.data());
    SmokeBuffer outputBuffer = createStorageBuffer(nullptr);
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
    } bufferGuard;
    bufferGuard.device = device;
    bufferGuard.input = inputBuffer;
    bufferGuard.output = outputBuffer;

    const std::vector<std::uint32_t>& shader = dummyComputeShaderSpirv();
    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = shader.size() * sizeof(std::uint32_t);
    shaderInfo.pCode = shader.data();

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    checkVk(vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule),
            "Vulkan wavefront smoke shader module creation");
    struct ShaderGuard {
      ~ShaderGuard() {
        if (shaderModule) {
          vkDestroyShaderModule(device, shaderModule, nullptr);
        }
      }

      VkDevice device{VK_NULL_HANDLE};
      VkShaderModule shaderModule{VK_NULL_HANDLE};
    } shaderGuard;
    shaderGuard.device = device;
    shaderGuard.shaderModule = shaderModule;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1] = bindings[0];
    bindings[1].binding = 1;

    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    descriptorLayoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    checkVk(vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr, &descriptorLayout),
            "Vulkan wavefront smoke descriptor layout creation");
    struct DescriptorLayoutGuard {
      ~DescriptorLayoutGuard() {
        if (layout) {
          vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }
      }

      VkDevice device{VK_NULL_HANDLE};
      VkDescriptorSetLayout layout{VK_NULL_HANDLE};
    } descriptorLayoutGuard;
    descriptorLayoutGuard.device = device;
    descriptorLayoutGuard.layout = descriptorLayout;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorLayout;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    checkVk(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout),
            "Vulkan wavefront smoke pipeline layout creation");
    struct PipelineLayoutGuard {
      ~PipelineLayoutGuard() {
        if (layout) {
          vkDestroyPipelineLayout(device, layout, nullptr);
        }
      }

      VkDevice device{VK_NULL_HANDLE};
      VkPipelineLayout layout{VK_NULL_HANDLE};
    } pipelineLayoutGuard;
    pipelineLayoutGuard.device = device;
    pipelineLayoutGuard.layout = pipelineLayout;

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = pipelineLayout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    checkVk(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline),
            "Vulkan wavefront smoke compute pipeline creation");
    struct PipelineGuard {
      ~PipelineGuard() {
        if (pipeline) {
          vkDestroyPipeline(device, pipeline, nullptr);
        }
      }

      VkDevice device{VK_NULL_HANDLE};
      VkPipeline pipeline{VK_NULL_HANDLE};
    } pipelineGuard;
    pipelineGuard.device = device;
    pipelineGuard.pipeline = pipeline;

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    checkVk(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool),
            "Vulkan wavefront smoke descriptor pool creation");
    struct DescriptorPoolGuard {
      ~DescriptorPoolGuard() {
        if (pool) {
          vkDestroyDescriptorPool(device, pool, nullptr);
        }
      }

      VkDevice device{VK_NULL_HANDLE};
      VkDescriptorPool pool{VK_NULL_HANDLE};
    } descriptorPoolGuard;
    descriptorPoolGuard.device = device;
    descriptorPoolGuard.pool = descriptorPool;

    VkDescriptorSetAllocateInfo descriptorAllocateInfo{};
    descriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocateInfo.descriptorPool = descriptorPool;
    descriptorAllocateInfo.descriptorSetCount = 1;
    descriptorAllocateInfo.pSetLayouts = &descriptorLayout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    checkVk(vkAllocateDescriptorSets(device, &descriptorAllocateInfo, &descriptorSet),
            "Vulkan wavefront smoke descriptor set allocation");

    VkDescriptorBufferInfo inputDescriptor{};
    inputDescriptor.buffer = inputBuffer.buffer;
    inputDescriptor.offset = 0;
    inputDescriptor.range = byteCount;
    VkDescriptorBufferInfo outputDescriptor{};
    outputDescriptor.buffer = outputBuffer.buffer;
    outputDescriptor.offset = 0;
    outputDescriptor.range = byteCount;

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].pBufferInfo = &inputDescriptor;
    descriptorWrites[1] = descriptorWrites[0];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].pBufferInfo = &outputDescriptor;
    vkUpdateDescriptorSets(device, static_cast<std::uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);

    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.queueFamilyIndex = selection.queueFamily;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    checkVk(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool),
            "Vulkan wavefront smoke command pool creation");
    struct CommandPoolGuard {
      ~CommandPoolGuard() {
        if (pool) {
          vkDestroyCommandPool(device, pool, nullptr);
        }
      }

      VkDevice device{VK_NULL_HANDLE};
      VkCommandPool pool{VK_NULL_HANDLE};
    } commandPoolGuard;
    commandPoolGuard.device = device;
    commandPoolGuard.pool = commandPool;

    VkCommandBufferAllocateInfo commandAllocateInfo{};
    commandAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandAllocateInfo.commandPool = commandPool;
    commandAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    checkVk(vkAllocateCommandBuffers(device, &commandAllocateInfo, &commandBuffer),
            "Vulkan wavefront smoke command buffer allocation");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    checkVk(vkBeginCommandBuffer(commandBuffer, &beginInfo),
            "Vulkan wavefront smoke command buffer begin");
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1,
                            &descriptorSet, 0, nullptr);
    vkCmdDispatch(commandBuffer, static_cast<std::uint32_t>(rayIds.size()), 1, 1);
    checkVk(vkEndCommandBuffer(commandBuffer), "Vulkan wavefront smoke command buffer end");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    checkVk(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE),
            "Vulkan wavefront smoke queue submit");
    checkVk(vkQueueWaitIdle(queue), "Vulkan wavefront smoke queue wait");

    std::vector<std::uint32_t> results(rayIds.size(), 0u);
    void* mapped = nullptr;
    checkVk(vkMapMemory(device, outputBuffer.memory, 0, byteCount, 0, &mapped),
            "Vulkan wavefront smoke output buffer mapping");
    std::memcpy(results.data(), mapped, static_cast<std::size_t>(byteCount));
    vkUnmapMemory(device, outputBuffer.memory);
    return results;
#else
    throw std::runtime_error("Vulkan wavefront backend is not enabled");
#endif
  }
}
