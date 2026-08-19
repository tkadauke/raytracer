#include "render/MetalTracingAccumulationKernel.h"

#include "render/TracingAccumulationReference.h"

#include "MetalComputeHelper.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace render {
  using render::detail::sharedMetalDevice;
  using render::detail::sharedCommandQueue;
  using render::detail::metalError;

  namespace {
    struct MetalFloat4 {
      float x{0.0f};
      float y{0.0f};
      float z{0.0f};
      float w{0.0f};
    };

    NSString* accumulationKernelSource() {
      return @"#include <metal_stdlib>\n"
              "using namespace metal;\n"
              "struct SampleColor {\n"
              "  float4 value;\n"
              "};\n"
              "uint packChannel(float value) {\n"
              "  return min(uint(max(value, 0.0f) * 255.0f), 255u);\n"
              "}\n"
              "kernel void clearAccumulation(device float4* colorSum [[buffer(0)]],\n"
              "                              device uint* sampleCount [[buffer(1)]],\n"
              "                              device float4* secondMoment [[buffer(2)]],\n"
              "                              constant uint& hasSecondMoment [[buffer(3)]],\n"
              "                              uint id [[thread_position_in_grid]]) {\n"
              "  colorSum[id] = float4(0.0f);\n"
              "  sampleCount[id] = 0u;\n"
              "  if (hasSecondMoment != 0u) {\n"
              "    secondMoment[id] = float4(0.0f);\n"
              "  }\n"
              "}\n"
              "kernel void addSampleColors(device float4* colorSum [[buffer(0)]],\n"
              "                            device uint* sampleCount [[buffer(1)]],\n"
              "                            device float4* secondMoment [[buffer(2)]],\n"
              "                            constant uint& hasSecondMoment [[buffer(3)]],\n"
              "                            device const SampleColor* samples [[buffer(4)]],\n"
              "                            uint id [[thread_position_in_grid]]) {\n"
              "  const float4 sample = samples[id].value;\n"
              "  colorSum[id] += float4(sample.rgb, 0.0f);\n"
              "  sampleCount[id] += 1u;\n"
              "  if (hasSecondMoment != 0u) {\n"
              "    secondMoment[id] += float4(sample.rgb * sample.rgb, 0.0f);\n"
              "  }\n"
              "}\n"
              "kernel void resolveAccumulation(device const float4* colorSum [[buffer(0)]],\n"
              "                               device const uint* sampleCount [[buffer(1)]],\n"
              "                               device uint* resolved [[buffer(2)]],\n"
              "                               uint id [[thread_position_in_grid]]) {\n"
              "  const uint count = sampleCount[id];\n"
              "  if (count == 0u) {\n"
              "    resolved[id] = 0u;\n"
              "    return;\n"
              "  }\n"
              "  const float3 color = colorSum[id].rgb / float(count);\n"
              "  resolved[id] = (packChannel(color.r) << 16u) | (packChannel(color.g) << 8u) |\n"
              "                 packChannel(color.b);\n"
              "}\n";
    }

    id<MTLComputePipelineState> newPipeline(id<MTLDevice> device, NSString* functionName) {
      NSError* error = nil;
      id<MTLLibrary> library = [device newLibraryWithSource:accumulationKernelSource()
                                                    options:nil
                                                      error:&error];
      if (!library) {
        throw metalError("Metal tracing accumulation shader compilation failed", error);
      }

      id<MTLFunction> function = [library newFunctionWithName:functionName];
      if (!function) {
        throw std::runtime_error("Metal tracing accumulation function was not found");
      }

      id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function
                                                                                   error:&error];
      if (!pipeline) {
        throw metalError("Metal tracing accumulation pipeline creation failed", error);
      }
      return pipeline;
    }

    id<MTLComputePipelineState> sharedClearPipeline() {
      static id<MTLComputePipelineState> pipeline =
        newPipeline(sharedMetalDevice(), @"clearAccumulation");
      return pipeline;
    }

    id<MTLComputePipelineState> sharedAddPipeline() {
      static id<MTLComputePipelineState> pipeline =
        newPipeline(sharedMetalDevice(), @"addSampleColors");
      return pipeline;
    }

    id<MTLComputePipelineState> sharedResolvePipeline() {
      static id<MTLComputePipelineState> pipeline =
        newPipeline(sharedMetalDevice(), @"resolveAccumulation");
      return pipeline;
    }

    void dispatchOneDimensional(id<MTLComputeCommandEncoder> encoder,
                                id<MTLComputePipelineState> pipeline, NSUInteger count) {
      const NSUInteger maxThreads =
        std::max<NSUInteger>(1, pipeline.maxTotalThreadsPerThreadgroup);
      const MTLSize gridSize = MTLSizeMake(count, 1, 1);
      const MTLSize threadgroupSize = MTLSizeMake(std::min<NSUInteger>(count, maxThreads), 1, 1);
      [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];
    }

    void waitFor(id<MTLCommandBuffer> commandBuffer, const char* context) {
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      if (commandBuffer.status == MTLCommandBufferStatusError) {
        throw metalError(context, commandBuffer.error);
      }
    }

    id<MTLBuffer> newBuffer(id<MTLDevice> device, std::uint64_t bytes) {
      return [device newBufferWithLength:static_cast<NSUInteger>(bytes)
                                 options:MTLResourceStorageModeShared];
    }

    MetalFloat4 toMetalFloat4(const Colord& color) {
      return MetalFloat4{static_cast<float>(color.r()), static_cast<float>(color.g()),
                         static_cast<float>(color.b()), 0.0f};
    }

    Colord toColor(const MetalFloat4& color) {
      return Colord(color.x, color.y, color.z);
    }

    std::vector<MetalFloat4> flattenColors(const Buffer<Colord>& colors) {
      std::vector<MetalFloat4> records(static_cast<std::size_t>(colors.width()) *
                                       static_cast<std::size_t>(colors.height()));
      std::size_t index = 0;
      for (int y = 0; y != colors.height(); ++y) {
        for (int x = 0; x != colors.width(); ++x) {
          records[index++] = toMetalFloat4(colors[y][x]);
        }
      }
      return records;
    }
  }

  bool MetalTracingAccumulationKernel::deviceAvailable() const {
    @autoreleasepool {
      return sharedMetalDevice() != nil;
    }
  }

  std::string MetalTracingAccumulationKernel::deviceUnavailableReason() const {
    @autoreleasepool {
      if (sharedMetalDevice()) {
        return "";
      }
      return "MTLCreateSystemDefaultDevice returned nil";
    }
  }

  bool MetalTracingAccumulationKernel::accumulationPathAvailable() const {
    return accumulationPathUnavailableReason().empty();
  }

  std::string MetalTracingAccumulationKernel::accumulationPathUnavailableReason() const {
    @autoreleasepool {
      if (!sharedMetalDevice()) {
        return deviceUnavailableReason();
      }
      if (!sharedCommandQueue()) {
        return "Metal default device did not create a command queue";
      }
      try {
        if (!sharedClearPipeline()) {
          return "Metal tracing accumulation clear pipeline was not created";
        }
        if (!sharedAddPipeline()) {
          return "Metal tracing accumulation add pipeline was not created";
        }
        if (!sharedResolvePipeline()) {
          return "Metal tracing accumulation resolve pipeline was not created";
        }
        return "";
      } catch (const std::exception& e) {
        return e.what();
      }
    }
  }

  struct MetalTracingAccumulationBuffer::Private {
    TracingAccumulationLayout layout;
    id<MTLDevice> device{nil};
    id<MTLCommandQueue> commandQueue{nil};
    id<MTLBuffer> colorSum{nil};
    id<MTLBuffer> sampleCount{nil};
    id<MTLBuffer> secondMoment{nil};
    id<MTLBuffer> resolved{nil};
    std::uint32_t hasSecondMoment{0u};
  };

  MetalTracingAccumulationBuffer::MetalTracingAccumulationBuffer(
    const TracingAccumulationLayout& layout)
      : p(std::make_unique<Private>()) {
    @autoreleasepool {
      p->layout = layout.validated();
      if (!MetalTracingAccumulationKernel().accumulationPathAvailable()) {
        throw std::runtime_error(MetalTracingAccumulationKernel().accumulationPathUnavailableReason());
      }

      p->device = sharedMetalDevice();
      p->commandQueue = sharedCommandQueue();
      p->hasSecondMoment = p->layout.hasMomentBuffer() ? 1u : 0u;

      p->colorSum = newBuffer(p->device, p->layout.colorSumBytes());
      p->sampleCount = newBuffer(p->device, p->layout.sampleCountBytes());
      p->secondMoment = p->hasSecondMoment ? newBuffer(p->device, p->layout.momentBytes()) : nil;
      p->resolved = newBuffer(p->device, p->layout.resolveBytes());
      if (!p->colorSum || !p->sampleCount || (p->hasSecondMoment && !p->secondMoment) ||
          !p->resolved) {
        throw std::runtime_error("Metal tracing accumulation buffer allocation failed");
      }
      clear();
    }
  }

  MetalTracingAccumulationBuffer::MetalTracingAccumulationBuffer(int width, int height)
      : MetalTracingAccumulationBuffer(TracingAccumulationLayout::image(width, height)) {
  }

  MetalTracingAccumulationBuffer::~MetalTracingAccumulationBuffer() = default;

  const TracingAccumulationLayout& MetalTracingAccumulationBuffer::layout() const {
    return p->layout;
  }

  void MetalTracingAccumulationBuffer::clear() {
    @autoreleasepool {
      id<MTLCommandBuffer> commandBuffer = [p->commandQueue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!commandBuffer || !encoder) {
        throw std::runtime_error("Metal tracing accumulation clear command setup failed");
      }

      [encoder setComputePipelineState:sharedClearPipeline()];
      [encoder setBuffer:p->colorSum offset:0 atIndex:0];
      [encoder setBuffer:p->sampleCount offset:0 atIndex:1];
      [encoder setBuffer:p->secondMoment ? p->secondMoment : p->colorSum offset:0 atIndex:2];
      [encoder setBytes:&p->hasSecondMoment length:sizeof(p->hasSecondMoment) atIndex:3];
      dispatchOneDimensional(encoder, sharedClearPipeline(),
                             static_cast<NSUInteger>(p->layout.pixelCount()));
      [encoder endEncoding];
      waitFor(commandBuffer, "Metal tracing accumulation clear dispatch failed");

      std::memset([p->resolved contents], 0, static_cast<std::size_t>(p->layout.resolveBytes()));
    }
  }

  void MetalTracingAccumulationBuffer::addSamples(const Buffer<Colord>& colors) {
    if (colors.width() != p->layout.width || colors.height() != p->layout.height) {
      throw std::invalid_argument(
        "Metal tracing accumulation sample color buffer dimensions do not match the layout");
    }

    @autoreleasepool {
      const std::vector<MetalFloat4> samples = flattenColors(colors);
      id<MTLBuffer> sampleBuffer = [p->device newBufferWithBytes:samples.data()
                                                          length:samples.size() *
                                                                 sizeof(MetalFloat4)
                                                         options:MTLResourceStorageModeShared];
      if (!sampleBuffer) {
        throw std::runtime_error("Metal tracing accumulation sample buffer allocation failed");
      }

      id<MTLCommandBuffer> commandBuffer = [p->commandQueue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!commandBuffer || !encoder) {
        throw std::runtime_error("Metal tracing accumulation add command setup failed");
      }

      [encoder setComputePipelineState:sharedAddPipeline()];
      [encoder setBuffer:p->colorSum offset:0 atIndex:0];
      [encoder setBuffer:p->sampleCount offset:0 atIndex:1];
      [encoder setBuffer:p->secondMoment ? p->secondMoment : p->colorSum offset:0 atIndex:2];
      [encoder setBytes:&p->hasSecondMoment length:sizeof(p->hasSecondMoment) atIndex:3];
      [encoder setBuffer:sampleBuffer offset:0 atIndex:4];
      dispatchOneDimensional(encoder, sharedAddPipeline(),
                             static_cast<NSUInteger>(p->layout.pixelCount()));
      [encoder endEncoding];
      waitFor(commandBuffer, "Metal tracing accumulation add dispatch failed");
    }
  }

  void MetalTracingAccumulationBuffer::resolve(Buffer<unsigned int>& target) const {
    if (target.width() != p->layout.width || target.height() != p->layout.height) {
      throw std::invalid_argument(
        "Metal tracing accumulation resolve target dimensions do not match the layout");
    }

    @autoreleasepool {
      id<MTLCommandBuffer> commandBuffer = [p->commandQueue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!commandBuffer || !encoder) {
        throw std::runtime_error("Metal tracing accumulation resolve command setup failed");
      }

      [encoder setComputePipelineState:sharedResolvePipeline()];
      [encoder setBuffer:p->colorSum offset:0 atIndex:0];
      [encoder setBuffer:p->sampleCount offset:0 atIndex:1];
      [encoder setBuffer:p->resolved offset:0 atIndex:2];
      dispatchOneDimensional(encoder, sharedResolvePipeline(),
                             static_cast<NSUInteger>(p->layout.pixelCount()));
      [encoder endEncoding];
      waitFor(commandBuffer, "Metal tracing accumulation resolve dispatch failed");

      const auto* resolvedPixels = static_cast<const unsigned int*>([p->resolved contents]);
      std::size_t index = 0;
      for (int y = 0; y != target.height(); ++y) {
        for (int x = 0; x != target.width(); ++x) {
          target[y][x] = resolvedPixels[index++];
        }
      }
    }
  }

  void MetalTracingAccumulationBuffer::copyTo(TracingAccumulationBuffer& target) const {
    if (target.layout().width != p->layout.width || target.layout().height != p->layout.height ||
        target.layout().momentFormat != p->layout.momentFormat) {
      throw std::invalid_argument(
        "Metal tracing accumulation copy target layout does not match the Metal layout");
    }

    const auto* colorSums = static_cast<const MetalFloat4*>([p->colorSum contents]);
    const auto* sampleCounts = static_cast<const std::uint32_t*>([p->sampleCount contents]);
    const auto* secondMoments =
      p->secondMoment ? static_cast<const MetalFloat4*>([p->secondMoment contents]) : nullptr;

    std::size_t index = 0;
    for (int y = 0; y != p->layout.height; ++y) {
      for (int x = 0; x != p->layout.width; ++x) {
        target.colorSum()[y][x] = toColor(colorSums[index]);
        target.sampleCount()[y][x] = sampleCounts[index];
        if (secondMoments && target.secondMoment()) {
          (*target.secondMoment())[y][x] = toColor(secondMoments[index]);
        }
        ++index;
      }
    }
  }
}
