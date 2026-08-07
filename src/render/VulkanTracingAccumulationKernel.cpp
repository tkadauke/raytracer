#include "render/VulkanTracingAccumulationKernel.h"

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
#include "render/VulkanTracingAccumulationAdd.generated.h"
#include "render/VulkanTracingAccumulationClear.generated.h"
#include "render/VulkanTracingAccumulationResolve.generated.h"
#include "VulkanComputeHelper.h"

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

    class VulkanAccumulationRuntime final : public render::detail::VulkanComputeHelper {
    public:
      VulkanAccumulationRuntime()
        : VulkanComputeHelper("Vulkan tracing accumulation",
                              "raytracer Vulkan tracing accumulation") {}

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

        BufferVectorGuard<SmokeBuffer> buffers;
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
      struct SmokeBuffer {
        VkBuffer buffer{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkDeviceSize byteCount{0};
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
          writeBuffer(device, result.memory, byteCount, initialData,
                      "Vulkan tracing accumulation input buffer mapping");
        }
        return result;
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
