#include "render/MetalGpuDiffusePathLoopKernel.h"

// macOS SDK headers still export a global Rect symbol. Shield the SDK spelling
// while importing Objective-C frameworks so project headers keep their Rect<T>.
#define Rect MacOSRect
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#undef Rect

#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace render {
  namespace {
    static_assert(std::is_standard_layout_v<GpuDiffusePathLoopLaunchParameters>,
                  "Metal diffuse path-loop launch parameters must stay shader ABI friendly");
    static_assert(sizeof(GpuDiffusePathLoopLaunchParameters) == 64);
    static_assert(alignof(GpuDiffusePathLoopLaunchParameters) == 16);

    id<MTLDevice> sharedMetalDevice() {
      static id<MTLDevice> device = MTLCreateSystemDefaultDevice();
      return device;
    }

    id<MTLCommandQueue> sharedCommandQueue() {
      static id<MTLCommandQueue> queue = [] {
        id<MTLDevice> device = sharedMetalDevice();
        return device ? [device newCommandQueue] : nil;
      }();
      return queue;
    }

    std::runtime_error metalError(const std::string& context, NSError* error) {
      std::string detail;
      if (error) {
        detail = [[error localizedDescription] UTF8String];
      }
      return std::runtime_error(detail.empty() ? context : context + ": " + detail);
    }

    double elapsedSeconds(std::chrono::steady_clock::time_point start,
                          std::chrono::steady_clock::time_point end) {
      return std::chrono::duration<double>(end - start).count();
    }

    NSString* diffusePathLoopKernelSource() {
      return @"#include <metal_stdlib>\n"
              "using namespace metal;\n"
              "struct GpuDiffusePathLoopLaunchParameters {\n"
              "  uint layoutVersion;\n"
              "  uint maxDepth;\n"
              "  uint russianRouletteDepth;\n"
              "  uint directLightSamples;\n"
              "  uint initialPathCount;\n"
              "  uint imageWidth;\n"
              "  uint imageHeight;\n"
              "  uint materialCount;\n"
              "  uint textureCount;\n"
              "  uint lightCount;\n"
              "  uint environmentCount;\n"
              "  uint debugIdCount;\n"
              "  uint bvhNodeCount;\n"
              "  uint primitiveCount;\n"
              "  uint transformCount;\n"
              "  uint reserved0;\n"
              "};\n"
              "kernel void probeDiffusePathLoopLaunch(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters [[buffer(0)]],\n"
              "    device GpuDiffusePathLoopLaunchParameters* echoedParameters [[buffer(1)]],\n"
              "    device const uchar* sceneUpload [[buffer(2)]],\n"
              "    device const uchar* initialPathStates [[buffer(3)]],\n"
              "    device uchar* activePathStates [[buffer(4)]],\n"
              "    device uchar* nextPathStates [[buffer(5)]],\n"
              "    device uchar* stepRecords [[buffer(6)]],\n"
              "    device uchar* retainedIndices [[buffer(7)]],\n"
              "    device uchar* accumulation [[buffer(8)]],\n"
              "    uint id [[thread_position_in_grid]]) {\n"
              "  if (id != 0u) {\n"
              "    return;\n"
              "  }\n"
              "  echoedParameters[0] = parameters;\n"
              "  activePathStates[0] = initialPathStates[0];\n"
              "  nextPathStates[0] = activePathStates[0];\n"
              "  stepRecords[0] = sceneUpload[0];\n"
              "  retainedIndices[0] = 0u;\n"
              "  accumulation[0] = 0u;\n"
              "}\n";
    }

    id<MTLComputePipelineState> newPipeline(id<MTLDevice> device) {
      NSError* error = nil;
      id<MTLLibrary> library = [device newLibraryWithSource:diffusePathLoopKernelSource()
                                                    options:nil
                                                      error:&error];
      if (!library) {
        throw metalError("Metal diffuse path-loop shader compilation failed", error);
      }

      id<MTLFunction> function = [library newFunctionWithName:@"probeDiffusePathLoopLaunch"];
      if (!function) {
        throw std::runtime_error("Metal diffuse path-loop launch probe function was not found");
      }

      id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function
                                                                                   error:&error];
      if (!pipeline) {
        throw metalError("Metal diffuse path-loop launch probe pipeline creation failed", error);
      }
      return pipeline;
    }

    id<MTLComputePipelineState> sharedLaunchProbePipeline() {
      static id<MTLComputePipelineState> pipeline = [] {
        id<MTLDevice> device = sharedMetalDevice();
        return device ? newPipeline(device) : nil;
      }();
      return pipeline;
    }

    NSUInteger bufferLength(std::uint64_t requestedBytes) {
      return static_cast<NSUInteger>(std::max<std::uint64_t>(1, requestedBytes));
    }
  }

  bool MetalGpuDiffusePathLoopKernel::deviceAvailable() const {
    @autoreleasepool {
      return sharedMetalDevice() != nil;
    }
  }

  std::string MetalGpuDiffusePathLoopKernel::deviceUnavailableReason() const {
    @autoreleasepool {
      if (sharedMetalDevice()) {
        return "";
      }
      return "MTLCreateSystemDefaultDevice returned nil";
    }
  }

  bool MetalGpuDiffusePathLoopKernel::launchPathAvailable() const {
    return launchPathUnavailableReason().empty();
  }

  std::string MetalGpuDiffusePathLoopKernel::launchPathUnavailableReason() const {
    @autoreleasepool {
      if (!sharedMetalDevice()) {
        return deviceUnavailableReason();
      }
      if (!sharedCommandQueue()) {
        return "Metal default device did not create a command queue";
      }
      try {
        if (!sharedLaunchProbePipeline()) {
          return "Metal diffuse path-loop launch probe pipeline was not created";
        }
        return "";
      } catch (const std::exception& e) {
        return e.what();
      }
    }
  }

  MetalGpuDiffusePathLoopKernelResult
  MetalGpuDiffusePathLoopKernel::runLaunchProbe(const GpuDiffusePathLoopLaunchPlan& plan) const {
    if (plan.parameters.layoutVersion != gpuDiffusePathLoopLaunchLayoutVersion) {
      throw std::invalid_argument("Metal diffuse path-loop launch descriptor version mismatch");
    }
    if (plan.parameters.maxDepth == 0) {
      throw std::invalid_argument("Metal diffuse path-loop launch requires positive max depth");
    }

    @autoreleasepool {
      if (!launchPathAvailable()) {
        throw std::runtime_error(launchPathUnavailableReason());
      }

      id<MTLDevice> device = sharedMetalDevice();
      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLComputePipelineState> pipeline = sharedLaunchProbePipeline();

      const auto uploadStart = std::chrono::steady_clock::now();
      id<MTLBuffer> parameterBuffer =
        [device newBufferWithBytes:&plan.parameters
                            length:sizeof(GpuDiffusePathLoopLaunchParameters)
                           options:MTLResourceStorageModeShared];
      id<MTLBuffer> echoedParameterBuffer =
        [device newBufferWithLength:sizeof(GpuDiffusePathLoopLaunchParameters)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> sceneUploadBuffer =
        [device newBufferWithLength:bufferLength(plan.buffers.sceneUploadBytes)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> initialPathBuffer =
        [device newBufferWithLength:bufferLength(plan.buffers.initialPathStateBytes)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> activePathBuffer =
        [device newBufferWithLength:bufferLength(plan.buffers.activePathStateBytes)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> nextPathBuffer =
        [device newBufferWithLength:bufferLength(plan.buffers.nextPathStateBytes)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> stepRecordBuffer =
        [device newBufferWithLength:bufferLength(plan.buffers.stepRecordBytes)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> retainedIndexBuffer =
        [device newBufferWithLength:bufferLength(plan.buffers.retainedIndexBytes)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> accumulationBuffer =
        [device newBufferWithLength:bufferLength(plan.buffers.accumulationBytes)
                            options:MTLResourceStorageModeShared];
      if (!parameterBuffer || !echoedParameterBuffer || !sceneUploadBuffer || !initialPathBuffer ||
          !activePathBuffer || !nextPathBuffer || !stepRecordBuffer || !retainedIndexBuffer ||
          !accumulationBuffer) {
        throw std::runtime_error("Metal diffuse path-loop launch buffer allocation failed");
      }
      MetalGpuDiffusePathLoopKernelResult result;
      result.bufferSizes = plan.buffers;
      result.uploadWorkerSeconds = elapsedSeconds(uploadStart, std::chrono::steady_clock::now());

      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!commandBuffer || !encoder) {
        throw std::runtime_error("Metal diffuse path-loop launch command setup failed");
      }

      [encoder setComputePipelineState:pipeline];
      [encoder setBuffer:parameterBuffer offset:0 atIndex:0];
      [encoder setBuffer:echoedParameterBuffer offset:0 atIndex:1];
      [encoder setBuffer:sceneUploadBuffer offset:0 atIndex:2];
      [encoder setBuffer:initialPathBuffer offset:0 atIndex:3];
      [encoder setBuffer:activePathBuffer offset:0 atIndex:4];
      [encoder setBuffer:nextPathBuffer offset:0 atIndex:5];
      [encoder setBuffer:stepRecordBuffer offset:0 atIndex:6];
      [encoder setBuffer:retainedIndexBuffer offset:0 atIndex:7];
      [encoder setBuffer:accumulationBuffer offset:0 atIndex:8];
      [encoder dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
      [encoder endEncoding];

      const auto kernelStart = std::chrono::steady_clock::now();
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      result.kernelWorkerSeconds = elapsedSeconds(kernelStart, std::chrono::steady_clock::now());
      if (commandBuffer.status == MTLCommandBufferStatusError) {
        throw metalError("Metal diffuse path-loop launch probe dispatch failed",
                         commandBuffer.error);
      }

      const auto readbackStart = std::chrono::steady_clock::now();
      std::memcpy(&result.echoedParameters, [echoedParameterBuffer contents],
                  sizeof(result.echoedParameters));
      result.readbackWorkerSeconds =
        elapsedSeconds(readbackStart, std::chrono::steady_clock::now());
      return result;
    }
  }
}
