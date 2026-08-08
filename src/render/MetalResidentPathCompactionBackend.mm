#include "render/MetalResidentPathCompactionBackend.h"

#include "render/MetalComputeHelper.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace render {
  using render::detail::sharedMetalDevice;
  using render::detail::sharedCommandQueue;
  using render::detail::metalError;

  namespace {
    static_assert(sizeof(GpuPathStateRecord) == 112);
    static_assert(alignof(GpuPathStateRecord) == 16);

    NSString* pathCompactionKernelSource() {
      return @"#include <metal_stdlib>\n"
              "using namespace metal;\n"
              "struct GpuPathStateRecord {\n"
              "  float4 origin;\n"
              "  float4 direction;\n"
              "  float4 throughput;\n"
              "  float4 accumulatedRadiance;\n"
              "  float4 continuation;\n"
              "  uint pixelIndex;\n"
              "  uint sampleIndex;\n"
              "  uint depth;\n"
              "  uint flags;\n"
              "  uint rngSeed;\n"
              "  uint reserved0;\n"
              "  uint reserved1;\n"
              "  uint reserved2;\n"
              "};\n"
              "kernel void compactPathStates(device const GpuPathStateRecord* source [[buffer(0)]],\n"
              "                              device const uint* retainedIndices [[buffer(1)]],\n"
              "                              device GpuPathStateRecord* compacted [[buffer(2)]],\n"
              "                              uint id [[thread_position_in_grid]]) {\n"
              "  compacted[id] = source[retainedIndices[id]];\n"
              "}\n";
    }

    id<MTLComputePipelineState> newPipeline(id<MTLDevice> device) {
      NSError* error = nil;
      id<MTLLibrary> library = [device newLibraryWithSource:pathCompactionKernelSource()
                                                    options:nil
                                                      error:&error];
      if (!library) {
        throw metalError("Metal resident path compaction shader compilation failed", error);
      }

      id<MTLFunction> function = [library newFunctionWithName:@"compactPathStates"];
      if (!function) {
        throw std::runtime_error("Metal resident path compaction function was not found");
      }

      id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function
                                                                                   error:&error];
      if (!pipeline) {
        throw metalError("Metal resident path compaction pipeline creation failed", error);
      }
      return pipeline;
    }

    id<MTLComputePipelineState> sharedPathCompactionPipeline() {
      static id<MTLComputePipelineState> pipeline = [] {
        id<MTLDevice> device = sharedMetalDevice();
        return device ? newPipeline(device) : nil;
      }();
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

    void validateDispatchCount(std::size_t retainedPathCount) {
      if (retainedPathCount > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error(
          "Metal resident path compaction retained path count exceeds shader index range");
      }
    }
  }

  bool MetalResidentPathCompactionBackend::deviceAvailable() const {
    @autoreleasepool {
      return sharedMetalDevice() != nil;
    }
  }

  std::string MetalResidentPathCompactionBackend::deviceUnavailableReason() const {
    @autoreleasepool {
      if (sharedMetalDevice()) {
        return "";
      }
      return "MTLCreateSystemDefaultDevice returned nil";
    }
  }

  bool MetalResidentPathCompactionBackend::compactionPathAvailable() const {
    return compactionPathUnavailableReason().empty();
  }

  std::string MetalResidentPathCompactionBackend::compactionPathUnavailableReason() const {
    @autoreleasepool {
      if (!sharedMetalDevice()) {
        return deviceUnavailableReason();
      }
      if (!sharedCommandQueue()) {
        return "Metal default device did not create a command queue";
      }
      try {
        if (!sharedPathCompactionPipeline()) {
          return "Metal resident path compaction pipeline was not created";
        }
        return "";
      } catch (const std::exception& e) {
        return e.what();
      }
    }
  }

  const char* MetalResidentPathCompactionBackend::name() const {
    return "metal_resident_path_compaction";
  }

  const char* MetalResidentPathCompactionBackend::pathStateResidency() const {
    return "metal_shared";
  }

  ResidentPathCompactionResult MetalResidentPathCompactionBackend::compact(
    const std::vector<GpuPathStateRecord>& sourceRecords,
    const std::vector<std::uint32_t>& retainedPathIndices) const {
    ResidentPathCompactionResult result;
    result.contract = ResidentPathCompactionContract::fromRetainedIndices(
      static_cast<std::uint64_t>(sourceRecords.size()), retainedPathIndices, name(),
      sizeof(GpuPathStateRecord));
    if (retainedPathIndices.empty()) {
      return result;
    }
    validateDispatchCount(retainedPathIndices.size());

    @autoreleasepool {
      if (!compactionPathAvailable()) {
        throw std::runtime_error(compactionPathUnavailableReason());
      }

      id<MTLDevice> device = sharedMetalDevice();
      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLComputePipelineState> pipeline = sharedPathCompactionPipeline();

      id<MTLBuffer> sourceBuffer =
        [device newBufferWithBytes:sourceRecords.data()
                            length:sourceRecords.size() * sizeof(GpuPathStateRecord)
                           options:MTLResourceStorageModeShared];
      id<MTLBuffer> retainedIndexBuffer =
        [device newBufferWithBytes:retainedPathIndices.data()
                            length:retainedPathIndices.size() * sizeof(std::uint32_t)
                           options:MTLResourceStorageModeShared];
      id<MTLBuffer> compactedBuffer =
        [device newBufferWithLength:retainedPathIndices.size() * sizeof(GpuPathStateRecord)
                            options:MTLResourceStorageModeShared];
      if (!sourceBuffer || !retainedIndexBuffer || !compactedBuffer) {
        throw std::runtime_error("Metal resident path compaction buffer allocation failed");
      }

      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!commandBuffer || !encoder) {
        throw std::runtime_error("Metal resident path compaction command setup failed");
      }

      [encoder setComputePipelineState:pipeline];
      [encoder setBuffer:sourceBuffer offset:0 atIndex:0];
      [encoder setBuffer:retainedIndexBuffer offset:0 atIndex:1];
      [encoder setBuffer:compactedBuffer offset:0 atIndex:2];
      dispatchOneDimensional(encoder, pipeline, retainedPathIndices.size());
      [encoder endEncoding];
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      if (commandBuffer.status == MTLCommandBufferStatusError) {
        throw metalError("Metal resident path compaction dispatch failed", commandBuffer.error);
      }

      result.retainedRecords.resize(retainedPathIndices.size());
      std::memcpy(result.retainedRecords.data(), [compactedBuffer contents],
                  result.retainedRecords.size() * sizeof(GpuPathStateRecord));
    }
    return result;
  }
}
