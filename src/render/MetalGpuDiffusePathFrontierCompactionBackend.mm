#include "render/MetalGpuDiffusePathFrontierCompactionBackend.h"

#include "MetalComputeHelper.h"
#include "render/TimingHelpers.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace render {
  using render::detail::sharedMetalDevice;
  using render::detail::metalDeviceAvailable;
  using render::detail::sharedCommandQueue;
  using render::detail::metalError;
  using render::detail::secondsBetween;

  namespace {
    static_assert(sizeof(GpuIntersectionRay) == 64);
    static_assert(alignof(GpuIntersectionRay) == 16);
    static_assert(sizeof(GpuDiffusePathStateRecord) == 160);
    static_assert(alignof(GpuDiffusePathStateRecord) == 16);

    NSString* diffuseFrontierCompactionKernelSource() {
      return @"#include <metal_stdlib>\n"
              "using namespace metal;\n"
              "struct GpuIntersectionRay {\n"
              "  float4 origin;\n"
              "  float4 direction;\n"
              "  float minDistance;\n"
              "  float maxDistance;\n"
              "  float timeSample;\n"
              "  uint flags;\n"
              "  uint rayIndex;\n"
              "  uint reserved0;\n"
              "  uint reserved1;\n"
              "  uint reserved2;\n"
              "};\n"
              "struct GpuDiffusePathStateRecord {\n"
              "  GpuIntersectionRay ray;\n"
              "  float4 throughput;\n"
              "  float4 accumulatedRadiance;\n"
              "  uint pixelIndex;\n"
              "  uint primarySampleIndex;\n"
              "  uint depth;\n"
              "  uint sampleSeed;\n"
              "  uint sampleDimensionBase;\n"
              "  uint sampleDimensionStride;\n"
              "  uint flags;\n"
              "  uint reserved0;\n"
              "  float previousBsdfPdf;\n"
              "  float previousLightPdf;\n"
              "  uint previousMaterial;\n"
              "  uint previousEventFlags;\n"
              "  uint reserved1;\n"
              "  uint reserved2;\n"
              "  uint reserved3;\n"
              "  uint reserved4;\n"
              "};\n"
              "kernel void compactDiffusePathFrontier(\n"
              "    device const GpuDiffusePathStateRecord* source [[buffer(0)]],\n"
              "    device const uint* retainedIndices [[buffer(1)]],\n"
              "    device GpuDiffusePathStateRecord* compacted [[buffer(2)]],\n"
              "    uint id [[thread_position_in_grid]]) {\n"
              "  compacted[id] = source[retainedIndices[id]];\n"
              "}\n";
    }

    id<MTLComputePipelineState> newPipeline(id<MTLDevice> device) {
      NSError* error = nil;
      id<MTLLibrary> library = [device newLibraryWithSource:diffuseFrontierCompactionKernelSource()
                                                    options:nil
                                                      error:&error];
      if (!library) {
        throw metalError("Metal diffuse frontier compaction shader compilation failed", error);
      }

      id<MTLFunction> function = [library newFunctionWithName:@"compactDiffusePathFrontier"];
      if (!function) {
        throw std::runtime_error("Metal diffuse frontier compaction function was not found");
      }

      id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function
                                                                                   error:&error];
      if (!pipeline) {
        throw metalError("Metal diffuse frontier compaction pipeline creation failed", error);
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

    void validateRetainedPathIndices(std::size_t inputPathCount,
                                     const std::vector<std::uint32_t>& retainedPathIndices) {
      std::uint32_t previous = 0;
      bool hasPrevious = false;
      for (const std::uint32_t index : retainedPathIndices) {
        if (index >= inputPathCount) {
          throw std::out_of_range(
            "Metal diffuse frontier compaction retained path index is out of range");
        }
        if (hasPrevious && index <= previous) {
          throw std::invalid_argument(
            "Metal diffuse frontier compaction retained path indices must be strictly increasing");
        }
        previous = index;
        hasPrevious = true;
      }
    }

    void validateDispatchCount(std::size_t retainedPathCount) {
      if (retainedPathCount > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error(
          "Metal diffuse frontier compaction retained path count exceeds shader index range");
      }
    }
  }

  bool MetalGpuDiffusePathFrontierCompactionBackend::deviceAvailable() const {
    return metalDeviceAvailable();
  }

  std::string MetalGpuDiffusePathFrontierCompactionBackend::deviceUnavailableReason() const {
    @autoreleasepool {
      if (sharedMetalDevice()) {
        return "";
      }
      return "MTLCreateSystemDefaultDevice returned nil";
    }
  }

  bool MetalGpuDiffusePathFrontierCompactionBackend::compactionPathAvailable() const {
    return compactionPathUnavailableReason().empty();
  }

  std::string
  MetalGpuDiffusePathFrontierCompactionBackend::compactionPathUnavailableReason() const {
    @autoreleasepool {
      if (!sharedMetalDevice()) {
        return deviceUnavailableReason();
      }
      if (!sharedCommandQueue()) {
        return "Metal default device did not create a command queue";
      }
      try {
        if (!sharedPathCompactionPipeline()) {
          return "Metal diffuse frontier compaction pipeline was not created";
        }
        return "";
      } catch (const std::exception& e) {
        return e.what();
      }
    }
  }

  const char* MetalGpuDiffusePathFrontierCompactionBackend::name() const {
    return "metal_diffuse_frontier_compaction";
  }

  const char* MetalGpuDiffusePathFrontierCompactionBackend::pathStateResidency() const {
    return "metal_shared_diffuse_path_state";
  }

  GpuDiffusePathFrontierCompactionResult MetalGpuDiffusePathFrontierCompactionBackend::compact(
    const std::vector<GpuDiffusePathStateRecord>& sourceRecords,
    const std::vector<std::uint32_t>& retainedPathIndices) const {
    validateRetainedPathIndices(sourceRecords.size(), retainedPathIndices);

    GpuDiffusePathFrontierCompactionResult result;
    result.executionPath = name();
    result.pathStateResidency = pathStateResidency();
    result.inputPathCount = sourceRecords.size();
    result.retainedPathIndices = retainedPathIndices;
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

      const auto uploadStart = std::chrono::steady_clock::now();
      id<MTLBuffer> sourceBuffer =
        [device newBufferWithBytes:sourceRecords.data()
                            length:sourceRecords.size() * sizeof(GpuDiffusePathStateRecord)
                           options:MTLResourceStorageModeShared];
      id<MTLBuffer> retainedIndexBuffer =
        [device newBufferWithBytes:retainedPathIndices.data()
                            length:retainedPathIndices.size() * sizeof(std::uint32_t)
                           options:MTLResourceStorageModeShared];
      id<MTLBuffer> compactedBuffer =
        [device newBufferWithLength:retainedPathIndices.size() *
                                    sizeof(GpuDiffusePathStateRecord)
                            options:MTLResourceStorageModeShared];
      if (!sourceBuffer || !retainedIndexBuffer || !compactedBuffer) {
        throw std::runtime_error("Metal diffuse frontier compaction buffer allocation failed");
      }
      result.uploadWorkerSeconds =
        secondsBetween(uploadStart, std::chrono::steady_clock::now());

      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!commandBuffer || !encoder) {
        throw std::runtime_error("Metal diffuse frontier compaction command setup failed");
      }

      [encoder setComputePipelineState:pipeline];
      [encoder setBuffer:sourceBuffer offset:0 atIndex:0];
      [encoder setBuffer:retainedIndexBuffer offset:0 atIndex:1];
      [encoder setBuffer:compactedBuffer offset:0 atIndex:2];
      dispatchOneDimensional(encoder, pipeline, retainedPathIndices.size());
      [encoder endEncoding];
      const auto kernelStart = std::chrono::steady_clock::now();
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      result.kernelWorkerSeconds =
        secondsBetween(kernelStart, std::chrono::steady_clock::now());
      if (commandBuffer.status == MTLCommandBufferStatusError) {
        throw metalError("Metal diffuse frontier compaction dispatch failed", commandBuffer.error);
      }

      const auto readbackStart = std::chrono::steady_clock::now();
      result.retainedRecords.resize(retainedPathIndices.size());
      std::memcpy(result.retainedRecords.data(), [compactedBuffer contents],
                  result.retainedRecords.size() * sizeof(GpuDiffusePathStateRecord));
      result.readbackWorkerSeconds =
        secondsBetween(readbackStart, std::chrono::steady_clock::now());
    }
    return result;
  }
}
