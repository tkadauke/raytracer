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
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace render {
  namespace {
    static_assert(std::is_standard_layout_v<GpuDiffusePathLoopLaunchParameters>,
                  "Metal diffuse path-loop launch parameters must stay shader ABI friendly");
    static_assert(sizeof(GpuDiffusePathLoopLaunchParameters) == 288);
    static_assert(alignof(GpuDiffusePathLoopLaunchParameters) == 16);
    static_assert(sizeof(GpuDiffusePathStateRecord) == 160);
    static_assert(alignof(GpuDiffusePathStateRecord) == 16);
    static_assert(sizeof(GpuDiffusePathStepRecord) == 96);
    static_assert(alignof(GpuDiffusePathStepRecord) == 16);
    static_assert(sizeof(GpuIntersectionHitRecord) == 112);
    static_assert(alignof(GpuIntersectionHitRecord) == 16);
    static_assert(sizeof(GpuTracingMaterialRecord) == 80);
    static_assert(alignof(GpuTracingMaterialRecord) == 16);
    static_assert(sizeof(GpuTracingTextureRecord) == 32);
    static_assert(alignof(GpuTracingTextureRecord) == 16);
    static_assert(sizeof(GpuTracingLightRecord) == 80);
    static_assert(alignof(GpuTracingLightRecord) == 16);
    static_assert(sizeof(GpuTracingEnvironmentRecord) == 32);
    static_assert(alignof(GpuTracingEnvironmentRecord) == 16);

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

    NSUInteger threadgroupWidthFor(id<MTLComputePipelineState> pipeline, NSUInteger threadCount) {
      if (threadCount == 0) {
        return 1;
      }
      NSUInteger width = pipeline.threadExecutionWidth;
      if (width == 0) {
        width = 64;
      }
      const NSUInteger maximumWidth = pipeline.maxTotalThreadsPerThreadgroup;
      if (maximumWidth != 0) {
        width = std::min(width, maximumWidth);
      }
      return std::max<NSUInteger>(1, std::min(width, threadCount));
    }

    void dispatch1D(id<MTLComputeCommandEncoder> encoder, id<MTLComputePipelineState> pipeline,
                    NSUInteger threadCount) {
      const NSUInteger threads = std::max<NSUInteger>(1, threadCount);
      [encoder dispatchThreads:MTLSizeMake(threads, 1, 1)
         threadsPerThreadgroup:MTLSizeMake(threadgroupWidthFor(pipeline, threads), 1, 1)];
    }

    NSString* diffusePathLoopKernelSourcePrefix() {
      return @"#include <metal_stdlib>\n"
              "using namespace metal;\n"
              "struct GpuDiffusePathLoopLaunchParameters {\n"
              "  uint layoutVersion;\n"
              "  uint maxDepth;\n"
              "  uint russianRouletteDepth;\n"
              "  uint directLightSamples;\n"
              "  uint captureDiagnostics;\n"
              "  uint initialPathCount;\n"
              "  uint imageWidth;\n"
              "  uint imageHeight;\n"
              "  uint materialCount;\n"
              "  uint textureCount;\n"
              "  uint lightCount;\n"
              "  uint environmentCount;\n"
              "  uint debugIdCount;\n"
              "  uint geometryByteOffset;\n"
              "  uint materialByteOffset;\n"
              "  uint textureByteOffset;\n"
              "  uint lightByteOffset;\n"
              "  uint environmentByteOffset;\n"
              "  uint debugIdByteOffset;\n"
              "  uint sceneUploadBytes;\n"
              "  uint accumulationTargetMode;\n"
              "  uint bvhByteOffset;\n"
              "  uint primitiveByteOffset;\n"
              "  uint triangleByteOffset;\n"
              "  uint sphereByteOffset;\n"
              "  uint planeByteOffset;\n"
              "  uint rectangleByteOffset;\n"
              "  uint diskByteOffset;\n"
              "  uint openCylinderByteOffset;\n"
              "  uint torusByteOffset;\n"
              "  uint transformByteOffset;\n"
              "  uint bvhNodeCount;\n"
              "  uint primitiveCount;\n"
              "  uint triangleCount;\n"
              "  uint sphereCount;\n"
              "  uint planeCount;\n"
              "  uint rectangleCount;\n"
              "  uint diskCount;\n"
              "  uint openCylinderCount;\n"
              "  uint torusCount;\n"
              "  uint transformCount;\n"
              "  uint primaryPathGenerationMode;\n"
              "  uint primaryPathSamplesPerPixel;\n"
              "  uint primaryPathSampleSeed;\n"
              "  uint primaryPathRequestedWidth;\n"
              "  int primaryPathRequestedLeft;\n"
              "  int primaryPathRequestedTop;\n"
              "  uint primaryPathRequestedHeight;\n"
              "  uint primaryPathActualWidth;\n"
              "  int primaryPathActualLeft;\n"
              "  int primaryPathActualTop;\n"
              "  uint primaryPathActualHeight;\n"
              "  uint reserved0;\n"
              "  uint reserved1;\n"
              "  uint reserved2;\n"
              "  uint reserved3;\n"
              "  float4 primaryPathOrigin;\n"
              "  float4 primaryPathTopLeft;\n"
              "  float4 primaryPathRight;\n"
              "  float4 primaryPathDown;\n"
              "};\n"
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
              "struct GpuDiffusePathStepRecord {\n"
              "  uint event;\n"
              "  uint pathIndex;\n"
              "  uint pixelIndex;\n"
              "  uint primarySampleIndex;\n"
              "  uint depth;\n"
              "  uint material;\n"
              "  uint object;\n"
              "  uint flags;\n"
              "  float4 emittedRadiance;\n"
              "  float4 directLightRadiance;\n"
              "  float4 missRadiance;\n"
              "  float4 continuationThroughput;\n"
              "};\n"
              "struct GpuTracingEnvironmentRecord {\n"
              "  uint texture;\n"
              "  uint flags;\n"
              "  uint2 reserved;\n"
              "  float4 color;\n"
              "};\n"
              "struct GpuTracingMaterialRecord {\n"
              "  uint kind;\n"
              "  uint albedoTexture;\n"
              "  uint emissionTexture;\n"
              "  uint flags;\n"
              "  float4 parameters;\n"
              "  float4 specularParameters;\n"
              "  float4 continuationParameters;\n"
              "  float4 transmissionParameters;\n"
              "};\n"
              "struct GpuTracingTextureRecord {\n"
              "  uint kind;\n"
              "  uint payloadOffset;\n"
              "  uint payloadCount;\n"
              "  uint flags;\n"
              "  float4 parameters;\n"
              "};\n"
              "struct GpuTracingLightRecord {\n"
              "  uint kind;\n"
              "  uint emissionTexture;\n"
              "  uint flags;\n"
              "  uint object;\n"
              "  float4 positionOrDirection;\n"
              "  float4 u;\n"
              "  float4 v;\n"
              "  float4 parameters;\n"
              "};\n"
              "struct DirectLightSelection {\n"
              "  uint valid;\n"
              "  uint lightIndex;\n"
              "  float pdf;\n"
              "};\n"
              "struct DirectLightSample {\n"
              "  uint valid;\n"
              "  uint delta;\n"
              "  float3 direction;\n"
              "  float4 radiance;\n"
              "  float distance;\n"
              "  float pdf;\n"
              "};\n"
              "struct GpuIntersectionBounds {\n"
              "  float4 minimum;\n"
              "  float4 maximum;\n"
              "};\n"
              "struct GpuIntersectionBvhNode {\n"
              "  GpuIntersectionBounds bounds;\n"
              "  uint leftOrFirstPrimitive;\n"
              "  uint primitiveCount;\n"
              "  uint flags;\n"
              "  uint reserved;\n"
              "};\n"
              "struct GpuIntersectionPrimitiveRecord {\n"
              "  GpuIntersectionBounds bounds;\n"
              "  uint kind;\n"
              "  uint material;\n"
              "  uint object;\n"
              "  uint transform;\n"
              "  uint payloadOffset;\n"
              "  uint payloadCount;\n"
              "  uint2 reserved;\n"
              "};\n"
              "struct GpuIntersectionTrianglePayload {\n"
              "  float4 point0;\n"
              "  float4 point1;\n"
              "  float4 point2;\n"
              "  float4 normal0;\n"
              "  float4 normal1;\n"
              "  float4 normal2;\n"
              "  float4 uv0;\n"
              "  float4 uv1;\n"
              "  float4 uv2;\n"
              "  float4 minimumHitDistance;\n"
              "};\n"
              "struct GpuIntersectionSpherePayload {\n"
              "  float4 centerRadius;\n"
              "};\n"
              "struct GpuIntersectionPlanePayload {\n"
              "  float4 normalDistance;\n"
              "};\n"
              "struct GpuIntersectionRectanglePayload {\n"
              "  float4 corner;\n"
              "  float4 leg1;\n"
              "  float4 leg2;\n"
              "  float4 normal;\n"
              "};\n"
              "struct GpuIntersectionDiskPayload {\n"
              "  float4 centerRadius;\n"
              "  float4 normalMinimumHitDistance;\n"
              "};\n"
              "struct GpuIntersectionOpenCylinderPayload {\n"
              "  float4 radiusHalfHeight;\n"
              "};\n"
              "struct GpuIntersectionTorusPayload {\n"
              "  float4 sweptTubeRadius;\n"
              "};\n"
              "struct GpuIntersectionTransformPayload {\n"
              "  float4 pointMatrix0;\n"
              "  float4 pointMatrix1;\n"
              "  float4 pointMatrix2;\n"
              "  float4 pointMatrix3;\n"
              "  float4 normalMatrix0;\n"
              "  float4 normalMatrix1;\n"
              "  float4 normalMatrix2;\n"
              "  float4 normalMatrix3;\n"
              "  float4 inversePointMatrix0;\n"
              "  float4 inversePointMatrix1;\n"
              "  float4 inversePointMatrix2;\n"
              "  float4 inversePointMatrix3;\n"
              "  float4 inverseDirectionMatrix0;\n"
              "  float4 inverseDirectionMatrix1;\n"
              "  float4 inverseDirectionMatrix2;\n"
              "  float4 inverseDirectionMatrix3;\n"
              "};\n"
              "struct GpuIntersectionHitRecord {\n"
              "  uint hit;\n"
              "  uint material;\n"
              "  uint object;\n"
              "  uint primitiveRecord;\n"
              "  uint rayIndex;\n"
              "  uint reservedId0;\n"
              "  uint reservedId1;\n"
              "  uint reservedId2;\n"
              "  float distance;\n"
              "  float reservedDistance0;\n"
              "  float reservedDistance1;\n"
              "  float reservedDistance2;\n"
              "  float4 point;\n"
              "  float4 normal;\n"
              "  float4 uv;\n"
              "  float4 barycentric;\n"
              "};\n"
              "struct LocalPrimitiveHit {\n"
              "  bool hit;\n"
              "  float distance;\n"
              "  float4 point;\n"
              "  float4 normal;\n"
              "  float4 uv;\n"
              "  float4 barycentric;\n"
              "};\n"
              "constant uint gpuIntersectionLeafNodeFlag = 1u;\n"
              "constant uint gpuIntersectionTrianglePrimitiveKind = 1u;\n"
              "constant uint gpuIntersectionSpherePrimitiveKind = 2u;\n"
              "constant uint gpuIntersectionPlanePrimitiveKind = 3u;\n"
              "constant uint gpuIntersectionRectanglePrimitiveKind = 4u;\n"
              "constant uint gpuIntersectionDiskPrimitiveKind = 5u;\n"
              "constant uint gpuIntersectionOpenCylinderPrimitiveKind = 6u;\n"
              "constant uint gpuIntersectionTorusPrimitiveKind = 7u;\n"
              "constant uint gpuTracingMatteMaterialKind = 1u;\n"
              "constant uint gpuTracingEmissiveMaterialKind = 2u;\n"
              "constant uint gpuTracingPhongMaterialKind = 3u;\n"
              "constant uint gpuTracingReflectiveMaterialKind = 4u;\n"
              "constant uint gpuTracingTransparentMaterialKind = 5u;\n"
              "constant uint gpuTracingConstantColorTextureKind = 1u;\n"
              "constant uint gpuTracingCheckerBoardTextureKind = 2u;\n"
              "constant uint gpuTracingImageTextureKind = 3u;\n"
              "constant uint gpuTracingTintedTextureKind = 4u;\n"
              "constant uint gpuTracingUvColorTextureKind = 5u;\n"
              "constant uint gpuTracingPlanarTextureMappingKind = 1u;\n"
              "constant uint gpuTracingUvTextureMappingKind = 2u;\n"
              "constant uint gpuTracingTextureMappingMask = 0xffu;\n"
              "constant uint gpuTracingTextureWrapClampFlag = 256u;\n"
              "constant uint gpuTracingTextureFilterBilinearFlag = 512u;\n"
              "constant uint gpuTracingPointLightKind = 1u;\n"
              "constant uint gpuTracingDirectionalLightKind = 2u;\n"
              "constant uint gpuTracingRectangularAreaLightKind = 3u;\n"
              "constant uint gpuPrimaryPathGenerationModePinhole = 1u;\n"
              "constant uint gpuDiffusePathStateActiveFlag = 1u;\n"
              "constant uint gpuDiffusePathStateTerminatedFlag = 2u;\n"
              "constant uint gpuDiffusePathStateSampledFromBsdfFlag = 4u;\n"
              "constant uint gpuDiffusePathStateBsdfSampleDeltaFlag = 8u;\n"
              "constant uint gpuDiffusePathStateUnsupportedFlag = 16u;\n"
              "constant uint gpuDiffusePathStepEventInactive = 0u;\n"
              "constant uint gpuDiffusePathStepEventMiss = 1u;\n"
              "constant uint gpuDiffusePathStepEventHit = 2u;\n"
              "constant uint gpuDiffusePathStepEventUnsupported = 3u;\n"
              "constant uint gpuDiffusePathLoopAccumulationTargetPath = 1u;\n"
              "constant uint gpuDiffusePathLoopAccumulationTargetSampleSlot = 2u;\n"
              "constant uint gpuSampleInitialCoordinateState = 0x811c9dc5u;\n"
              "constant uint gpuSampleCoordinateStep = 0x9e3779b9u;\n"
              "constant uint gpuPrimaryPathSampleDimensionBase = 3u;\n"
              "constant uint gpuPrimaryPathSampleDimensionStride = 4u;\n"
              "constant float pathLoopInvPi = 0.31830988618379067154f;\n"
              "constant float pathLoopTau = 6.28318530717958647692f;\n"
              "constant float pathLoopRayEpsilon = 1.0e-7f;\n"
              "constant float pathLoopMinimumHitDistance = 1.0e-4f;\n"
              "constant float pathLoopMinimumContinuationProbability = 0.05f;\n"
              "constant uint pathLoopMaxTextureEvaluationDepth = 8u;\n"
              "float finiteInfinity() {\n"
              "  return 3.4028234663852886e+38f;\n"
              "}\n"
              "float rayInfinity() {\n"
              "  return as_type<float>(0x7f800000u);\n"
              "}\n"
              "bool pathStateIsActive(const GpuDiffusePathStateRecord path) {\n"
              "  return (path.flags & gpuDiffusePathStateActiveFlag) != 0u &&\n"
              "         (path.flags & gpuDiffusePathStateTerminatedFlag) == 0u;\n"
              "}\n"
              "bool pathStateIsTerminated(const GpuDiffusePathStateRecord path) {\n"
              "  return (path.flags & gpuDiffusePathStateTerminatedFlag) != 0u;\n"
              "}\n"
              "uint activePathFlags(uint flags) {\n"
              "  return (flags | gpuDiffusePathStateActiveFlag) &\n"
              "         ~gpuDiffusePathStateTerminatedFlag;\n"
              "}\n"
              "uint terminatedPathFlags(uint flags) {\n"
              "  return (flags | gpuDiffusePathStateTerminatedFlag) &\n"
              "         ~gpuDiffusePathStateActiveFlag;\n"
              "}\n"
              "uint sampleDimension(const GpuDiffusePathStateRecord path, uint offset) {\n"
              "  return path.sampleDimensionBase + path.depth * path.sampleDimensionStride + offset;\n"
              "}\n"
              "uint pcgHash32(uint input) {\n"
              "  const uint state = input * 747796405u + 2891336453u;\n"
              "  const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;\n"
              "  return (word >> 22u) ^ word;\n"
              "}\n"
              "uint mixSampleCoordinateWord(uint state, uint value) {\n"
              "  return pcgHash32(state ^\n"
              "                   (value + gpuSampleCoordinateStep + (state << 6u) + (state >> 2u)));\n"
              "}\n"
              "uint sampleHash(uint seed, uint pixelIndex, uint primarySampleIndex,\n"
              "                uint dimension, uint component) {\n"
              "  uint state = gpuSampleInitialCoordinateState;\n"
              "  state = mixSampleCoordinateWord(state, seed);\n"
              "  state = mixSampleCoordinateWord(state, pixelIndex);\n"
              "  state = mixSampleCoordinateWord(state, primarySampleIndex);\n"
              "  state = mixSampleCoordinateWord(state, dimension);\n"
              "  state = mixSampleCoordinateWord(state, component);\n"
              "  return pcgHash32(state);\n"
              "}\n"
              "float sample1D(const GpuDiffusePathStateRecord path, uint dimension, uint component) {\n"
              "  return float(sampleHash(path.sampleSeed, path.pixelIndex,\n"
              "                          path.primarySampleIndex, dimension, component) >> 8u) *\n"
              "         (1.0f / 16777216.0f);\n"
              "}\n"
              "float2 sample2D(const GpuDiffusePathStateRecord path, uint dimension) {\n"
              "  return float2(sample1D(path, dimension, 0u), sample1D(path, dimension, 1u));\n"
              "}\n"
              "float sample1DCoordinate(uint seed, uint pixelIndex, uint primarySampleIndex,\n"
              "                         uint dimension, uint component) {\n"
              "  return float(sampleHash(seed, pixelIndex, primarySampleIndex,\n"
              "                          dimension, component) >> 8u) *\n"
              "         (1.0f / 16777216.0f);\n"
              "}\n"
              "float2 sample2DCoordinate(uint seed, uint pixelIndex, uint primarySampleIndex,\n"
              "                          uint dimension) {\n"
              "  return float2(sample1DCoordinate(seed, pixelIndex, primarySampleIndex,\n"
              "                                  dimension, 0u),\n"
              "                sample1DCoordinate(seed, pixelIndex, primarySampleIndex,\n"
              "                                  dimension, 1u));\n"
              "}\n"
              "GpuDiffusePathStateRecord makePinholePrimaryPath(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters, uint pathIndex) {\n"
              "  const uint sampleIndex = pathIndex % parameters.primaryPathSamplesPerPixel;\n"
              "  const uint pixelOrdinal = pathIndex / parameters.primaryPathSamplesPerPixel;\n"
              "  const uint localX = pixelOrdinal % parameters.primaryPathActualWidth;\n"
              "  const uint localY = pixelOrdinal / parameters.primaryPathActualWidth;\n"
              "  const int column = parameters.primaryPathActualLeft + int(localX);\n"
              "  const int row = parameters.primaryPathActualTop + int(localY);\n"
              "  const uint pixelIndex = uint(\n"
              "      (row - parameters.primaryPathRequestedTop) *\n"
              "          int(parameters.primaryPathRequestedWidth) +\n"
              "      (column - parameters.primaryPathRequestedLeft));\n"
              "  const float2 pixelSample = sample2DCoordinate(\n"
              "      parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 0u);\n"
              "  const float timeSample = sample1DCoordinate(\n"
              "      parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 1u, 0u);\n"
              "  const float4 pixelPoint =\n"
              "      parameters.primaryPathTopLeft +\n"
              "      parameters.primaryPathRight * (float(column) + pixelSample.x) +\n"
              "      parameters.primaryPathDown * (float(row) + pixelSample.y);\n"
              "  GpuDiffusePathStateRecord path;\n"
              "  path.ray.origin = parameters.primaryPathOrigin;\n"
              "  path.ray.direction = float4(\n"
              "      normalize(pixelPoint.xyz - parameters.primaryPathOrigin.xyz), 0.0f);\n"
              "  path.ray.minDistance = 0.0f;\n"
              "  path.ray.maxDistance = rayInfinity();\n"
              "  path.ray.timeSample = timeSample;\n"
              "  path.ray.flags = 0u;\n"
              "  path.ray.rayIndex = pathIndex;\n"
              "  path.ray.reserved0 = 0u;\n"
              "  path.ray.reserved1 = 0u;\n"
              "  path.ray.reserved2 = 0u;\n"
              "  path.throughput = float4(1.0f, 1.0f, 1.0f, 0.0f);\n"
              "  path.accumulatedRadiance = float4(0.0f);\n"
              "  path.pixelIndex = pixelIndex;\n"
              "  path.primarySampleIndex = sampleIndex;\n"
              "  path.depth = 0u;\n"
              "  path.sampleSeed = parameters.primaryPathSampleSeed;\n"
              "  path.sampleDimensionBase = gpuPrimaryPathSampleDimensionBase;\n"
              "  path.sampleDimensionStride = gpuPrimaryPathSampleDimensionStride;\n"
              "  path.flags = gpuDiffusePathStateActiveFlag;\n"
              "  path.reserved0 = 0u;\n"
              "  path.previousBsdfPdf = 0.0f;\n"
              "  path.previousLightPdf = 0.0f;\n"
              "  path.previousMaterial = 0u;\n"
              "  path.previousEventFlags = 0u;\n"
              "  path.reserved1 = 0u;\n"
              "  path.reserved2 = 0u;\n"
              "  path.reserved3 = 0u;\n"
              "  path.reserved4 = 0u;\n"
              "  return path;\n"
              "}\n"
              "GpuDiffusePathStateRecord initialPathFor(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const GpuDiffusePathStateRecord* initialPathStates, uint pathIndex) {\n"
              "  if (parameters.primaryPathGenerationMode == gpuPrimaryPathGenerationModePinhole) {\n"
              "    return makePinholePrimaryPath(parameters, pathIndex);\n"
              "  }\n"
              "  return initialPathStates[pathIndex];\n"
              "}\n"
              "uint lightSelectionSampleIndex(uint bounce, uint directSampleIndex) {\n"
              "  const uint sum = bounce + directSampleIndex;\n"
              "  return sum * (sum + 1u) / 2u + directSampleIndex;\n"
              "}\n"
              "uint lightSampleIndex(uint bounce, uint lightIndex, uint directSampleIndex) {\n"
              "  const uint effectiveBounce = lightSelectionSampleIndex(bounce, directSampleIndex);\n"
              "  const uint sum = effectiveBounce + lightIndex;\n"
              "  return sum * (sum + 1u) / 2u + lightIndex;\n"
              "}\n"
              "uint lightSelectionDimension(GpuDiffusePathStateRecord path, uint directSampleIndex) {\n"
              "  return 5u + lightSelectionSampleIndex(path.depth, directSampleIndex) * 4u;\n"
              "}\n"
              "uint lightSurfaceDimension(GpuDiffusePathStateRecord path, uint lightIndex,\n"
              "                           uint directSampleIndex) {\n"
              "  return 4u + lightSampleIndex(path.depth, lightIndex, directSampleIndex) * 4u;\n"
              "}\n"
              "float maxColor(float4 color) {\n"
              "  return max(max(color.x, color.y), color.z);\n"
              "}\n"
              "float3 tangentFor(float3 normal) {\n"
              "  const float3 helper = abs(normal.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) :\n"
              "                                                    float3(1.0f, 0.0f, 0.0f);\n"
              "  return normalize(cross(helper, normal));\n"
              "}\n"
              "float3 cosineHemisphereDirection(float3 normal, float2 sample) {\n"
              "  const float u0 = clamp(sample.x, 0.0f, 1.0f);\n"
              "  const float u1 = clamp(sample.y, 0.0f, 1.0f);\n"
              "  const float r = sqrt(u0);\n"
              "  const float phi = pathLoopTau * u1;\n"
              "  const float x = r * cos(phi);\n"
              "  const float y = r * sin(phi);\n"
              "  const float z = sqrt(max(0.0f, 1.0f - u0));\n"
              "  const float3 tangent = tangentFor(normal);\n"
              "  const float3 bitangent = cross(normal, tangent);\n"
              "  return normalize(tangent * x + bitangent * y + normal * z);\n"
              "}\n"
              "float3 phongLobeDirection(float3 axis, float2 sample, float exponent) {\n"
              "  const float u0 = clamp(sample.x, 0.0f, 1.0f);\n"
              "  const float u1 = clamp(sample.y, 0.0f, 1.0f);\n"
              "  const float cosTheta = pow(u0, 1.0f / (exponent + 1.0f));\n"
              "  const float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));\n"
              "  const float phi = pathLoopTau * u1;\n"
              "  const float3 tangent = tangentFor(axis);\n"
              "  const float3 bitangent = cross(axis, tangent);\n"
              "  return normalize(tangent * (sinTheta * cos(phi)) +\n"
              "                   bitangent * (sinTheta * sin(phi)) + axis * cosTheta);\n"
              "}\n"
              "float cosineHemispherePdf(float3 normal, float3 direction) {\n"
              "  const float normalDotDirection = dot(normal, direction);\n"
              "  return normalDotDirection <= 0.0f ? 0.0f : normalDotDirection * pathLoopInvPi;\n"
              "}\n"
              "float continuationProbability(float4 throughput) {\n"
              "  const float maximum = max(max(throughput.x, throughput.y), throughput.z);\n"
              "  if (maximum <= 0.0f) {\n"
              "    return 0.0f;\n"
              "  }\n"
              "  return clamp(maximum, pathLoopMinimumContinuationProbability, 1.0f);\n"
              "}\n"
              "bool throughputIsBlack(float4 throughput) {\n"
              "  return throughput.x <= 0.0f && throughput.y <= 0.0f && throughput.z <= 0.0f;\n"
              "}\n"
              "bool boundsIntersectsRay(const GpuIntersectionBounds bounds,\n"
              "                         const GpuIntersectionRay ray,\n"
              "                         float maxHitDistance) {\n"
              "  float enter = ray.minDistance;\n"
              "  float exit = min(ray.maxDistance, maxHitDistance);\n"
              "  if (exit < enter) {\n"
              "    return false;\n"
              "  }\n"
              "  for (uint axis = 0u; axis != 3u; ++axis) {\n"
              "    const float origin = ray.origin[axis];\n"
              "    const float direction = ray.direction[axis];\n"
              "    const float minimum = bounds.minimum[axis];\n"
              "    const float maximum = bounds.maximum[axis];\n"
              "    if (abs(direction) <= 1.1920928955078125e-7f) {\n"
              "      if (origin < minimum || origin > maximum) {\n"
              "        return false;\n"
              "      }\n"
              "      continue;\n"
              "    }\n"
              "    const float inverseDirection = 1.0f / direction;\n"
              "    float nearDistance = (minimum - origin) * inverseDirection;\n"
              "    float farDistance = (maximum - origin) * inverseDirection;\n"
              "    if (nearDistance > farDistance) {\n"
              "      const float swapDistance = nearDistance;\n"
              "      nearDistance = farDistance;\n"
              "      farDistance = swapDistance;\n"
              "    }\n"
              "    enter = max(enter, nearDistance);\n"
              "    exit = min(exit, farDistance);\n"
              "    if (exit < enter) {\n"
              "      return false;\n"
              "    }\n"
              "  }\n"
              "  return true;\n"
              "}\n"
              "float4 normalize3(float4 value) {\n"
              "  const float lengthSquared = dot(value.xyz, value.xyz);\n"
              "  if (lengthSquared <= pathLoopRayEpsilon) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  return float4(value.xyz * rsqrt(lengthSquared), 0.0f);\n"
              "}\n"
              "float4 transformPoint(float4 row0, float4 row1, float4 row2, float4 row3,\n"
              "                      float4 point) {\n"
              "  return float4(dot(row0, point), dot(row1, point), dot(row2, point),\n"
              "                dot(row3, point));\n"
              "}\n"
              "float4 transformDirection(float4 row0, float4 row1, float4 row2,\n"
              "                          float4 direction) {\n"
              "  return float4(dot(row0.xyz, direction.xyz), dot(row1.xyz, direction.xyz),\n"
              "                dot(row2.xyz, direction.xyz), 0.0f);\n"
              "}\n"
              "GpuIntersectionRay transformRay(GpuIntersectionRay ray,\n"
              "                                GpuIntersectionTransformPayload transform) {\n"
              "  GpuIntersectionRay result = ray;\n"
              "  result.origin = transformPoint(transform.inversePointMatrix0,\n"
              "                                 transform.inversePointMatrix1,\n"
              "                                 transform.inversePointMatrix2,\n"
              "                                 transform.inversePointMatrix3, ray.origin);\n"
              "  result.direction = transformDirection(transform.inverseDirectionMatrix0,\n"
              "                                      transform.inverseDirectionMatrix1,\n"
              "                                      transform.inverseDirectionMatrix2,\n"
              "                                      ray.direction);\n"
              "  return result;\n"
              "}\n"
              "LocalPrimitiveHit transformHit(LocalPrimitiveHit hit,\n"
              "                                GpuIntersectionTransformPayload transform) {\n"
              "  if (!hit.hit) {\n"
              "    return hit;\n"
              "  }\n"
              "  hit.point = transformPoint(transform.pointMatrix0, transform.pointMatrix1,\n"
              "                             transform.pointMatrix2, transform.pointMatrix3,\n"
              "                             hit.point);\n"
              "  hit.normal = normalize3(transformDirection(transform.normalMatrix0,\n"
              "                                            transform.normalMatrix1,\n"
              "                                            transform.normalMatrix2,\n"
              "                                            hit.normal));\n"
              "  return hit;\n"
              "}\n"
              "LocalPrimitiveHit emptyPrimitiveHit(float maxHitDistance) {\n"
              "  LocalPrimitiveHit result;\n"
              "  result.hit = false;\n"
              "  result.distance = maxHitDistance;\n"
              "  result.point = float4(0.0f);\n"
              "  result.normal = float4(0.0f);\n"
              "  result.uv = float4(0.0f);\n"
              "  result.barycentric = float4(0.0f);\n"
              "  return result;\n"
              "}\n"
              "LocalPrimitiveHit intersectTriangle(const GpuIntersectionRay ray,\n"
              "                                 const GpuIntersectionTrianglePayload triangle,\n"
              "                                 float maxHitDistance) {\n"
              "  LocalPrimitiveHit result = emptyPrimitiveHit(maxHitDistance);\n"
              "  const float a = triangle.point0.x - triangle.point1.x;\n"
              "  const float b = triangle.point0.x - triangle.point2.x;\n"
              "  const float c = ray.direction.x;\n"
              "  const float d = triangle.point0.x - ray.origin.x;\n"
              "  const float e = triangle.point0.y - triangle.point1.y;\n"
              "  const float f = triangle.point0.y - triangle.point2.y;\n"
              "  const float g = ray.direction.y;\n"
              "  const float h = triangle.point0.y - ray.origin.y;\n"
              "  const float i = triangle.point0.z - triangle.point1.z;\n"
              "  const float j = triangle.point0.z - triangle.point2.z;\n"
              "  const float k = ray.direction.z;\n"
              "  const float l = triangle.point0.z - ray.origin.z;\n"
              "  const float m = f * k - g * j;\n"
              "  const float n = h * k - g * l;\n"
              "  const float p = f * l - h * j;\n"
              "  const float q = g * i - e * k;\n"
              "  const float r = e * l - h * i;\n"
              "  const float s = e * j - f * i;\n"
              "  const float denominator = a * m + b * q + c * s;\n"
              "  if (denominator == 0.0f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float inverseDenominator = 1.0f / denominator;\n"
              "  const float beta = (d * m - b * n - c * p) * inverseDenominator;\n"
              "  if (beta < 0.0f || beta > 1.0f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float gamma = (a * n + d * q + c * r) * inverseDenominator;\n"
              "  if (gamma < 0.0f || gamma > 1.0f || beta + gamma > 1.0f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float distance = (a * p - b * r + d * s) * inverseDenominator;\n"
              "  const float minimumDistance = max(ray.minDistance, triangle.minimumHitDistance.x);\n"
              "  if (distance < minimumDistance || distance > ray.maxDistance ||\n"
              "      distance >= maxHitDistance) {\n"
              "    return result;\n"
              "  }\n"
              "  const float alpha = 1.0f - beta - gamma;\n"
              "  result.hit = true;\n"
              "  result.distance = distance;\n"
              "  result.point = float4(ray.origin.xyz + ray.direction.xyz * distance, 1.0f);\n"
              "  result.normal = float4(normalize(triangle.normal0.xyz * alpha +\n"
              "                                  triangle.normal1.xyz * beta +\n"
              "                                  triangle.normal2.xyz * gamma), 0.0f);\n"
              "  result.uv = triangle.uv0 * alpha + triangle.uv1 * beta + triangle.uv2 * gamma;\n"
              "  result.barycentric = float4(alpha, beta, gamma, 0.0f);\n"
              "  return result;\n"
              "}\n"
              "LocalPrimitiveHit intersectSphere(const GpuIntersectionRay ray,\n"
              "                               const GpuIntersectionSpherePayload sphere,\n"
              "                               float maxHitDistance) {\n"
              "  LocalPrimitiveHit result = emptyPrimitiveHit(maxHitDistance);\n"
              "  const float3 center = sphere.centerRadius.xyz;\n"
              "  const float radius = sphere.centerRadius.w;\n"
              "  const float3 origin = ray.origin.xyz - center;\n"
              "  const float3 direction = ray.direction.xyz;\n"
              "  const float od = dot(origin, direction);\n"
              "  const float dd = dot(direction, direction);\n"
              "  if (dd <= 1.1920928955078125e-7f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float discriminant = od * od - dd * (dot(origin, origin) - radius * radius);\n"
              "  if (discriminant <= 0.0f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float root = sqrt(discriminant);\n"
              "  const float nearDistance = (-od - root) / dd;\n"
              "  const float farDistance = (-od + root) / dd;\n"
              "  if (nearDistance <= 0.0f && farDistance <= 0.0f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float distance = nearDistance >= ray.minDistance ? nearDistance : farDistance;\n"
              "  if (distance < ray.minDistance || distance > ray.maxDistance || distance >= maxHitDistance) {\n"
              "    return result;\n"
              "  }\n"
              "  result.hit = true;\n"
              "  result.distance = distance;\n"
              "  const float3 point = ray.origin.xyz + ray.direction.xyz * distance;\n"
              "  result.point = float4(point, 1.0f);\n"
              "  result.normal = float4(normalize(point - center), 0.0f);\n"
              "  return result;\n"
              "}\n"
              "LocalPrimitiveHit intersectPlane(const GpuIntersectionRay ray,\n"
              "                              const GpuIntersectionPlanePayload plane,\n"
              "                              float maxHitDistance) {\n"
              "  LocalPrimitiveHit result = emptyPrimitiveHit(maxHitDistance);\n"
              "  const float3 normal = plane.normalDistance.xyz;\n"
              "  const float planeDistance = plane.normalDistance.w;\n"
              "  const float angle = dot(normal, ray.direction.xyz);\n"
              "  if (abs(angle) <= 1.1920928955078125e-7f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float distance = -(dot(normal, ray.origin.xyz) + planeDistance) / angle;\n"
              "  if (distance <= 0.0f || distance < ray.minDistance || distance > ray.maxDistance ||\n"
              "      distance >= maxHitDistance) {\n"
              "    return result;\n"
              "  }\n"
              "  result.hit = true;\n"
              "  result.distance = distance;\n"
              "  result.point = float4(ray.origin.xyz + ray.direction.xyz * distance, 1.0f);\n"
              "  result.normal = float4(normal, 0.0f);\n"
              "  return result;\n"
              "}\n"
              "LocalPrimitiveHit intersectRectangle(const GpuIntersectionRay ray,\n"
              "                                  const GpuIntersectionRectanglePayload rectangle,\n"
              "                                  float maxHitDistance) {\n"
              "  LocalPrimitiveHit result = emptyPrimitiveHit(maxHitDistance);\n"
              "  const float3 normal = rectangle.normal.xyz;\n"
              "  const float denominator = dot(ray.direction.xyz, normal);\n"
              "  if (denominator == 0.0f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float distance = dot(rectangle.corner.xyz - ray.origin.xyz, normal) / denominator;\n"
              "  if (!isfinite(distance) || distance < 0.0f || distance < ray.minDistance ||\n"
              "      distance > ray.maxDistance || distance >= maxHitDistance) {\n"
              "    return result;\n"
              "  }\n"
              "  const float3 point = ray.origin.xyz + ray.direction.xyz * distance;\n"
              "  const float3 difference = point - rectangle.corner.xyz;\n"
              "  const float dot1 = dot(difference, rectangle.leg1.xyz);\n"
              "  const float squaredLength1 = dot(rectangle.leg1.xyz, rectangle.leg1.xyz);\n"
              "  if (dot1 < 0.0f || dot1 > squaredLength1) {\n"
              "    return result;\n"
              "  }\n"
              "  const float dot2 = dot(difference, rectangle.leg2.xyz);\n"
              "  const float squaredLength2 = dot(rectangle.leg2.xyz, rectangle.leg2.xyz);\n"
              "  if (dot2 < 0.0f || dot2 > squaredLength2) {\n"
              "    return result;\n"
              "  }\n"
              "  result.hit = true;\n"
              "  result.distance = distance;\n"
              "  result.point = float4(point, 1.0f);\n"
              "  result.normal = float4(normal, 0.0f);\n"
              "  return result;\n"
              "}\n"
              "LocalPrimitiveHit intersectDisk(const GpuIntersectionRay ray,\n"
              "                             const GpuIntersectionDiskPayload disk,\n"
              "                             float maxHitDistance) {\n"
              "  LocalPrimitiveHit result = emptyPrimitiveHit(maxHitDistance);\n"
              "  const float3 center = disk.centerRadius.xyz;\n"
              "  const float radius = disk.centerRadius.w;\n"
              "  const float3 normal = disk.normalMinimumHitDistance.xyz;\n"
              "  const float minimumHitDistance = disk.normalMinimumHitDistance.w;\n"
              "  const float denominator = dot(ray.direction.xyz, normal);\n"
              "  if (denominator == 0.0f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float distance = dot(center - ray.origin.xyz, normal) / denominator;\n"
              "  if (!isfinite(distance) || distance < minimumHitDistance || distance < ray.minDistance ||\n"
              "      distance > ray.maxDistance || distance >= maxHitDistance) {\n"
              "    return result;\n"
              "  }\n"
              "  const float3 point = ray.origin.xyz + ray.direction.xyz * distance;\n"
              "  const float3 hitOffset = point - center;\n"
              "  if (!(dot(hitOffset, hitOffset) < radius * radius)) {\n"
              "    return result;\n"
              "  }\n"
              "  result.hit = true;\n"
              "  result.distance = distance;\n"
              "  result.point = float4(point, 1.0f);\n"
              "  result.normal = float4(normal, 0.0f);\n"
              "  return result;\n"
              "}\n"
              "LocalPrimitiveHit intersectOpenCylinder(\n"
              "    const GpuIntersectionRay ray,\n"
              "    const GpuIntersectionOpenCylinderPayload openCylinder,\n"
              "    float maxHitDistance) {\n"
              "  LocalPrimitiveHit result = emptyPrimitiveHit(maxHitDistance);\n"
              "  const float radius = openCylinder.radiusHalfHeight.x;\n"
              "  const float halfHeight = openCylinder.radiusHalfHeight.y;\n"
              "  const float inverseRadius = openCylinder.radiusHalfHeight.z;\n"
              "  const float a = ray.direction.x * ray.direction.x +\n"
              "                  ray.direction.z * ray.direction.z;\n"
              "  if (abs(a) <= pathLoopRayEpsilon) {\n"
              "    return result;\n"
              "  }\n"
              "  const float b = 2.0f * (ray.origin.x * ray.direction.x +\n"
              "                          ray.origin.z * ray.direction.z);\n"
              "  const float c = ray.origin.x * ray.origin.x + ray.origin.z * ray.origin.z -\n"
              "                  radius * radius;\n"
              "  const float determinant = b * b - 4.0f * a * c;\n"
              "  if (determinant <= pathLoopRayEpsilon) {\n"
              "    return result;\n"
              "  }\n"
              "  const float determinantRoot = sqrt(determinant);\n"
              "  const float denominator = 2.0f * a;\n"
              "  const float distances[2] = {\n"
              "    (-determinantRoot - b) / denominator,\n"
              "    (determinantRoot - b) / denominator,\n"
              "  };\n"
              "  float bestDistance = finiteInfinity();\n"
              "  for (uint index = 0u; index != 2u; ++index) {\n"
              "    const float distance = distances[index];\n"
              "    if (distance <= 0.0f || distance < ray.minDistance ||\n"
              "        distance > ray.maxDistance || distance >= bestDistance ||\n"
              "        distance >= maxHitDistance) {\n"
              "      continue;\n"
              "    }\n"
              "    const float y = ray.origin.y + ray.direction.y * distance;\n"
              "    if (y < -halfHeight || y > halfHeight) {\n"
              "      continue;\n"
              "    }\n"
              "    bestDistance = distance;\n"
              "  }\n"
              "  if (!isfinite(bestDistance)) {\n"
              "    return result;\n"
              "  }\n"
              "  result.hit = true;\n"
              "  result.distance = bestDistance;\n"
              "  result.point = float4(ray.origin.xyz + ray.direction.xyz * bestDistance, 1.0f);\n"
              "  result.normal = float4(result.point.x * inverseRadius, 0.0f,\n"
              "                         result.point.z * inverseRadius, 0.0f);\n"
              "  float u = atan2(result.point.z, result.point.x) / pathLoopTau;\n"
              "  if (u < 0.0f) {\n"
              "    u += 1.0f;\n"
              "  }\n"
              "  const float height = 2.0f * halfHeight;\n"
              "  const float v = height == 0.0f ? 0.0f : (result.point.y + halfHeight) / height;\n"
              "  result.uv = float4(u, v, 0.0f, 0.0f);\n"
              "  return result;\n"
              "}\n"
              "bool almostZero(float value) {\n"
              "  return abs(value) <= pathLoopRayEpsilon * 10.0f;\n"
              "}\n"
              "float cubeRoot(float value) {\n"
              "  return value < 0.0f ? -pow(-value, 1.0f / 3.0f) : pow(value, 1.0f / 3.0f);\n"
              "}\n"
              "uint solveQuadric(float a, float b, float c, thread float* roots) {\n"
              "  if (almostZero(a)) {\n"
              "    if (almostZero(b)) {\n"
              "      return 0u;\n"
              "    }\n"
              "    roots[0] = -c / b;\n"
              "    return 1u;\n"
              "  }\n"
              "  const float determinant = b * b - 4.0f * a * c;\n"
              "  if (almostZero(determinant)) {\n"
              "    roots[0] = -b / (2.0f * a);\n"
              "    return 1u;\n"
              "  }\n"
              "  if (determinant > 0.0f) {\n"
              "    const float determinantRoot = sqrt(determinant);\n"
              "    roots[0] = (-determinantRoot - b) / (2.0f * a);\n"
              "    roots[1] = (determinantRoot - b) / (2.0f * a);\n"
              "    return 2u;\n"
              "  }\n"
              "  return 0u;\n"
              "}\n"
              "uint solveCubic(float a, float b, float c, float d, thread float* roots) {\n"
              "  if (almostZero(a)) {\n"
              "    return solveQuadric(b, c, d, roots);\n"
              "  }\n"
              "  const float normA = b / a;\n"
              "  const float normB = c / a;\n"
              "  const float normC = d / a;\n"
              "  const float normASquared = normA * normA;\n"
              "  const float p = (-(1.0f / 3.0f) * normASquared + normB) / 3.0f;\n"
              "  const float q = (2.0f / 27.0f * normA * normASquared -\n"
              "                   (1.0f / 3.0f) * normA * normB + normC) / 2.0f;\n"
              "  const float pCube = p * p * p;\n"
              "  const float determinant = q * q + pCube;\n"
              "  uint count = 0u;\n"
              "  if (almostZero(determinant)) {\n"
              "    if (almostZero(q)) {\n"
              "      roots[0] = 0.0f;\n"
              "      count = 1u;\n"
              "    } else {\n"
              "      const float root = cubeRoot(-q);\n"
              "      roots[0] = 2.0f * root;\n"
              "      roots[1] = -root;\n"
              "      count = 2u;\n"
              "    }\n"
              "  } else if (determinant < 0.0f) {\n"
              "    const float pi = 3.14159265358979323846f;\n"
              "    const float phi = acos(clamp(-q / sqrt(-pCube), -1.0f, 1.0f)) / 3.0f;\n"
              "    const float t = 2.0f * sqrt(-p);\n"
              "    roots[0] = t * cos(phi);\n"
              "    roots[1] = -t * cos(phi + pi / 3.0f);\n"
              "    roots[2] = -t * cos(phi - pi / 3.0f);\n"
              "    count = 3u;\n"
              "  } else {\n"
              "    const float determinantRoot = sqrt(determinant);\n"
              "    roots[0] = cubeRoot(determinantRoot - q) - cubeRoot(determinantRoot + q);\n"
              "    count = 1u;\n"
              "  }\n"
              "  const float sub = normA / 3.0f;\n"
              "  for (uint index = 0u; index != count; ++index) {\n"
              "    roots[index] -= sub;\n"
              "  }\n"
              "  return count;\n"
              "}\n"
              "uint solveQuartic(float a, float b, float c, float d, float e,\n"
              "                  thread float* roots) {\n"
              "  if (almostZero(a)) {\n"
              "    return solveCubic(b, c, d, e, roots);\n"
              "  }\n"
              "  const float normA = b / a;\n"
              "  const float normB = c / a;\n"
              "  const float normC = d / a;\n"
              "  const float normD = e / a;\n"
              "  const float normASquared = normA * normA;\n"
              "  const float p = -3.0f / 8.0f * normASquared + normB;\n"
              "  const float q = 1.0f / 8.0f * normASquared * normA -\n"
              "                  0.5f * normA * normB + normC;\n"
              "  const float r = -3.0f / 256.0f * normASquared * normASquared +\n"
              "                  1.0f / 16.0f * normASquared * normB -\n"
              "                  0.25f * normA * normC + normD;\n"
              "  uint count = 0u;\n"
              "  if (almostZero(r)) {\n"
              "    thread float cubicRoots[3];\n"
              "    const uint cubicCount = solveCubic(1.0f, 0.0f, p, q, cubicRoots);\n"
              "    for (uint index = 0u; index != cubicCount; ++index) {\n"
              "      roots[count++] = cubicRoots[index];\n"
              "    }\n"
              "  } else {\n"
              "    thread float cubicRoots[3];\n"
              "    const uint cubicCount = solveCubic(1.0f, -0.5f * p, -r,\n"
              "                                       0.5f * r * p - 0.125f * q * q,\n"
              "                                       cubicRoots);\n"
              "    if (cubicCount == 0u) {\n"
              "      return 0u;\n"
              "    }\n"
              "    const float z = cubicRoots[0];\n"
              "    float u = z * z - r;\n"
              "    float v = 2.0f * z - p;\n"
              "    const float uTol = pathLoopRayEpsilon * 16.0f * (1.0f + abs(z * z) + abs(r));\n"
              "    const float vTol = pathLoopRayEpsilon * 16.0f * (1.0f + abs(2.0f * z) + abs(p));\n"
              "    if (u < -uTol || v < -vTol) {\n"
              "      return 0u;\n"
              "    }\n"
              "    u = u <= 0.0f ? 0.0f : sqrt(u);\n"
              "    v = v <= 0.0f ? 0.0f : sqrt(v);\n"
              "    thread float quadRoots[2];\n"
              "    uint quadCount = solveQuadric(1.0f, q < 0.0f ? -v : v, z - u, quadRoots);\n"
              "    for (uint index = 0u; index != quadCount; ++index) {\n"
              "      roots[count++] = quadRoots[index];\n"
              "    }\n"
              "    quadCount = solveQuadric(1.0f, q < 0.0f ? v : -v, z + u, quadRoots);\n"
              "    for (uint index = 0u; index != quadCount; ++index) {\n"
              "      roots[count++] = quadRoots[index];\n"
              "    }\n"
              "  }\n"
              "  const float sub = 0.25f * normA;\n"
              "  for (uint index = 0u; index != count; ++index) {\n"
              "    roots[index] -= sub;\n"
              "  }\n"
              "  for (uint i = 1u; i < count; ++i) {\n"
              "    const float value = roots[i];\n"
              "    uint j = i;\n"
              "    while (j > 0u && roots[j - 1u] > value) {\n"
              "      roots[j] = roots[j - 1u];\n"
              "      --j;\n"
              "    }\n"
              "    roots[j] = value;\n"
              "  }\n"
              "  return count;\n"
              "}\n"
              "LocalPrimitiveHit intersectTorus(\n"
              "    const GpuIntersectionRay ray,\n"
              "    const GpuIntersectionTorusPayload torus,\n"
              "    float maxHitDistance) {\n"
              "  LocalPrimitiveHit result = emptyPrimitiveHit(maxHitDistance);\n"
              "  const float sweptRadius = torus.sweptTubeRadius.x;\n"
              "  const float tubeRadius = torus.sweptTubeRadius.y;\n"
              "  const float dd = dot(ray.direction.xyz, ray.direction.xyz);\n"
              "  const float oorr = dot(ray.origin.xyz, ray.origin.xyz) -\n"
              "                     sweptRadius * sweptRadius - tubeRadius * tubeRadius;\n"
              "  const float od = dot(ray.origin.xyz, ray.direction.xyz);\n"
              "  const float fourRR = 4.0f * sweptRadius * sweptRadius;\n"
              "  thread float roots[4];\n"
              "  const uint rootCount = solveQuartic(\n"
              "    dd * dd,\n"
              "    4.0f * dd * od,\n"
              "    2.0f * dd * oorr + 4.0f * od * od + fourRR * ray.direction.y * ray.direction.y,\n"
              "    4.0f * od * oorr + 2.0f * fourRR * ray.origin.y * ray.direction.y,\n"
              "    oorr * oorr - fourRR * (tubeRadius * tubeRadius - ray.origin.y * ray.origin.y),\n"
              "    roots);\n"
              "  float bestDistance = finiteInfinity();\n"
              "  for (uint index = 0u; index != rootCount; ++index) {\n"
              "    const float distance = roots[index];\n"
              "    if (distance <= 0.0f || distance < ray.minDistance ||\n"
              "        distance > ray.maxDistance || distance >= bestDistance ||\n"
              "        distance >= maxHitDistance) {\n"
              "      continue;\n"
              "    }\n"
              "    bestDistance = distance;\n"
              "  }\n"
              "  if (!isfinite(bestDistance)) {\n"
              "    return result;\n"
              "  }\n"
              "  result.hit = true;\n"
              "  result.distance = bestDistance;\n"
              "  result.point = float4(ray.origin.xyz + ray.direction.xyz * bestDistance, 1.0f);\n"
              "  const float paramSquared = sweptRadius * sweptRadius + tubeRadius * tubeRadius;\n"
              "  const float sumSquared = dot(result.point.xyz, result.point.xyz);\n"
              "  result.normal = float4(normalize(float3(\n"
              "    4.0f * result.point.x * (sumSquared - paramSquared),\n"
              "    4.0f * result.point.y *\n"
              "      (sumSquared - paramSquared + 2.0f * sweptRadius * sweptRadius),\n"
              "    4.0f * result.point.z * (sumSquared - paramSquared))), 0.0f);\n"
              "  return result;\n"
              "}\n"
              "GpuIntersectionHitRecord missHitRecord(const GpuIntersectionRay ray) {\n"
              "  GpuIntersectionHitRecord hit;\n"
              "  hit.hit = 0u;\n"
              "  hit.material = 0u;\n"
              "  hit.object = 0u;\n"
              "  hit.primitiveRecord = 0u;\n"
              "  hit.rayIndex = ray.rayIndex;\n"
              "  hit.reservedId0 = 0u;\n"
              "  hit.reservedId1 = 0u;\n"
              "  hit.reservedId2 = 0u;\n"
              "  hit.distance = finiteInfinity();\n"
              "  hit.reservedDistance0 = 0.0f;\n"
              "  hit.reservedDistance1 = 0.0f;\n"
              "  hit.reservedDistance2 = 0.0f;\n"
              "  hit.point = float4(0.0f);\n"
              "  hit.normal = float4(0.0f);\n"
              "  hit.uv = float4(0.0f);\n"
              "  hit.barycentric = float4(0.0f);\n"
              "  return hit;\n"
              "}\n"
              "GpuIntersectionHitRecord hitRecordForPrimitive(\n"
              "    const GpuIntersectionRay ray,\n"
              "    const GpuIntersectionPrimitiveRecord primitive,\n"
              "    uint primitiveRecordIndex,\n"
              "    const LocalPrimitiveHit primitiveHit) {\n"
              "  GpuIntersectionHitRecord hit = missHitRecord(ray);\n"
              "  hit.hit = 1u;\n"
              "  hit.material = primitive.material;\n"
              "  hit.object = primitive.object;\n"
              "  hit.primitiveRecord = primitiveRecordIndex;\n"
              "  hit.distance = primitiveHit.distance;\n"
              "  hit.point = primitiveHit.point;\n"
              "  hit.normal = primitiveHit.normal;\n"
              "  hit.uv = primitiveHit.uv;\n"
              "  hit.barycentric = primitiveHit.barycentric;\n"
              "  return hit;\n"
              "}\n"
              "GpuIntersectionHitRecord closestSupportedHit(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    const GpuIntersectionRay ray) {\n"
              "  GpuIntersectionHitRecord closest = missHitRecord(ray);\n"
              "  if (parameters.bvhNodeCount == 0u || parameters.primitiveCount == 0u ||\n"
              "      (parameters.triangleCount == 0u && parameters.sphereCount == 0u &&\n"
              "       parameters.planeCount == 0u && parameters.rectangleCount == 0u &&\n"
              "       parameters.diskCount == 0u && parameters.openCylinderCount == 0u &&\n"
              "       parameters.torusCount == 0u)) {\n"
              "    return closest;\n"
              "  }\n"
              "  device const GpuIntersectionBvhNode* bvh =\n"
              "      reinterpret_cast<device const GpuIntersectionBvhNode*>(\n"
              "          sceneUpload + parameters.bvhByteOffset);\n"
              "  device const GpuIntersectionPrimitiveRecord* primitives =\n"
              "      reinterpret_cast<device const GpuIntersectionPrimitiveRecord*>(\n"
              "          sceneUpload + parameters.primitiveByteOffset);\n"
              "  device const GpuIntersectionTrianglePayload* triangles =\n"
              "      reinterpret_cast<device const GpuIntersectionTrianglePayload*>(\n"
              "          sceneUpload + parameters.triangleByteOffset);\n"
              "  device const GpuIntersectionSpherePayload* spheres =\n"
              "      reinterpret_cast<device const GpuIntersectionSpherePayload*>(\n"
              "          sceneUpload + parameters.sphereByteOffset);\n"
              "  device const GpuIntersectionPlanePayload* planes =\n"
              "      reinterpret_cast<device const GpuIntersectionPlanePayload*>(\n"
              "          sceneUpload + parameters.planeByteOffset);\n"
              "  device const GpuIntersectionRectanglePayload* rectangles =\n"
              "      reinterpret_cast<device const GpuIntersectionRectanglePayload*>(\n"
              "          sceneUpload + parameters.rectangleByteOffset);\n"
              "  device const GpuIntersectionDiskPayload* disks =\n"
              "      reinterpret_cast<device const GpuIntersectionDiskPayload*>(\n"
              "          sceneUpload + parameters.diskByteOffset);\n"
              "  device const GpuIntersectionOpenCylinderPayload* openCylinders =\n"
              "      reinterpret_cast<device const GpuIntersectionOpenCylinderPayload*>(\n"
              "          sceneUpload + parameters.openCylinderByteOffset);\n"
              "  device const GpuIntersectionTorusPayload* tori =\n"
              "      reinterpret_cast<device const GpuIntersectionTorusPayload*>(\n"
              "          sceneUpload + parameters.torusByteOffset);\n"
              "  device const GpuIntersectionTransformPayload* transforms =\n"
              "      reinterpret_cast<device const GpuIntersectionTransformPayload*>(\n"
              "          sceneUpload + parameters.transformByteOffset);\n"
              "  uint stack[64];\n"
              "  uint stackSize = 0u;\n"
              "  stack[stackSize++] = 0u;\n"
              "  while (stackSize != 0u) {\n"
              "    const uint nodeIndex = stack[--stackSize];\n"
              "    if (nodeIndex >= parameters.bvhNodeCount) {\n"
              "      continue;\n"
              "    }\n"
              "    const GpuIntersectionBvhNode node = bvh[nodeIndex];\n"
              "    if (!boundsIntersectsRay(node.bounds, ray, closest.distance)) {\n"
              "      continue;\n"
              "    }\n"
              "    if ((node.flags & gpuIntersectionLeafNodeFlag) == 0u) {\n"
              "      if (stackSize + 2u <= 64u) {\n"
              "        stack[stackSize++] = node.primitiveCount;\n"
              "        stack[stackSize++] = node.leftOrFirstPrimitive;\n"
              "      }\n"
              "      continue;\n"
              "    }\n"
              "    for (uint offset = 0u; offset != node.primitiveCount; ++offset) {\n"
              "      const uint primitiveIndex = node.leftOrFirstPrimitive + offset;\n"
              "      if (primitiveIndex >= parameters.primitiveCount) {\n"
              "        continue;\n"
              "      }\n"
              "      const GpuIntersectionPrimitiveRecord primitive = primitives[primitiveIndex];\n"
              "      if (!boundsIntersectsRay(primitive.bounds, ray, closest.distance)) {\n"
              "        continue;\n"
              "      }\n"
              "      GpuIntersectionRay primitiveRay = ray;\n"
              "      bool hasTransform = primitive.transform != 0u;\n"
              "      GpuIntersectionTransformPayload transform;\n"
              "      if (hasTransform) {\n"
              "        if (primitive.transform >= parameters.transformCount) {\n"
              "          continue;\n"
              "        }\n"
              "        transform = transforms[primitive.transform];\n"
              "        primitiveRay = transformRay(ray, transform);\n"
              "      }\n"
              "      if (primitive.kind == gpuIntersectionTrianglePrimitiveKind) {\n"
              "        if (primitive.payloadOffset >= parameters.triangleCount) {\n"
              "          continue;\n"
              "        }\n"
              "        LocalPrimitiveHit triangleHit = intersectTriangle(\n"
              "            primitiveRay, triangles[primitive.payloadOffset], closest.distance);\n"
              "        if (hasTransform) {\n"
              "          triangleHit = transformHit(triangleHit, transform);\n"
              "        }\n"
              "        if (triangleHit.hit) {\n"
              "          closest = hitRecordForPrimitive(ray, primitive, primitiveIndex, triangleHit);\n"
              "        }\n"
              "        continue;\n"
              "      }\n"
              "      if (primitive.kind == gpuIntersectionSpherePrimitiveKind) {\n"
              "        if (primitive.payloadOffset >= parameters.sphereCount) {\n"
              "          continue;\n"
              "        }\n"
              "        LocalPrimitiveHit sphereHit = intersectSphere(\n"
              "            primitiveRay, spheres[primitive.payloadOffset], closest.distance);\n"
              "        if (hasTransform) {\n"
              "          sphereHit = transformHit(sphereHit, transform);\n"
              "        }\n"
              "        if (sphereHit.hit) {\n"
              "          closest = hitRecordForPrimitive(ray, primitive, primitiveIndex, sphereHit);\n"
              "        }\n"
              "        continue;\n"
              "      }\n"
              "      if (primitive.kind == gpuIntersectionPlanePrimitiveKind) {\n"
              "        if (primitive.payloadOffset >= parameters.planeCount) {\n"
              "          continue;\n"
              "        }\n"
              "        LocalPrimitiveHit planeHit = intersectPlane(\n"
              "            primitiveRay, planes[primitive.payloadOffset], closest.distance);\n"
              "        if (hasTransform) {\n"
              "          planeHit = transformHit(planeHit, transform);\n"
              "        }\n"
              "        if (planeHit.hit) {\n"
              "          closest = hitRecordForPrimitive(ray, primitive, primitiveIndex, planeHit);\n"
              "        }\n"
              "        continue;\n"
              "      }\n"
              "      if (primitive.kind == gpuIntersectionRectanglePrimitiveKind) {\n"
              "        if (primitive.payloadOffset >= parameters.rectangleCount) {\n"
              "          continue;\n"
              "        }\n"
              "        LocalPrimitiveHit rectangleHit = intersectRectangle(\n"
              "            primitiveRay, rectangles[primitive.payloadOffset], closest.distance);\n"
              "        if (hasTransform) {\n"
              "          rectangleHit = transformHit(rectangleHit, transform);\n"
              "        }\n"
              "        if (rectangleHit.hit) {\n"
              "          closest = hitRecordForPrimitive(ray, primitive, primitiveIndex, rectangleHit);\n"
              "        }\n"
              "        continue;\n"
              "      }\n"
              "      if (primitive.kind == gpuIntersectionDiskPrimitiveKind) {\n"
              "        if (primitive.payloadOffset >= parameters.diskCount) {\n"
              "          continue;\n"
              "        }\n"
              "        LocalPrimitiveHit diskHit = intersectDisk(\n"
              "            primitiveRay, disks[primitive.payloadOffset], closest.distance);\n"
              "        if (hasTransform) {\n"
              "          diskHit = transformHit(diskHit, transform);\n"
              "        }\n"
              "        if (diskHit.hit) {\n"
              "          closest = hitRecordForPrimitive(ray, primitive, primitiveIndex, diskHit);\n"
              "        }\n"
              "        continue;\n"
              "      }\n"
              "      if (primitive.kind == gpuIntersectionOpenCylinderPrimitiveKind) {\n"
              "        if (primitive.payloadOffset >= parameters.openCylinderCount) {\n"
              "          continue;\n"
              "        }\n"
              "        LocalPrimitiveHit openCylinderHit = intersectOpenCylinder(\n"
              "            primitiveRay, openCylinders[primitive.payloadOffset], closest.distance);\n"
              "        if (hasTransform) {\n"
              "          openCylinderHit = transformHit(openCylinderHit, transform);\n"
              "        }\n"
              "        if (openCylinderHit.hit) {\n"
              "          closest = hitRecordForPrimitive(\n"
              "              ray, primitive, primitiveIndex, openCylinderHit);\n"
              "        }\n"
              "        continue;\n"
              "      }\n"
              "      if (primitive.kind == gpuIntersectionTorusPrimitiveKind) {\n"
              "        if (primitive.payloadOffset >= parameters.torusCount) {\n"
              "          continue;\n"
              "        }\n"
              "        LocalPrimitiveHit torusHit = intersectTorus(\n"
              "            primitiveRay, tori[primitive.payloadOffset], closest.distance);\n"
              "        if (hasTransform) {\n"
              "          torusHit = transformHit(torusHit, transform);\n"
              "        }\n"
              "        if (torusHit.hit) {\n"
              "          closest = hitRecordForPrimitive(ray, primitive, primitiveIndex, torusHit);\n"
              "        }\n"
              "      }\n"
              "    }\n"
              "  }\n"
              "  return closest;\n"
              "}\n"
              "float2 textureCoordinates(GpuTracingTextureRecord texture,\n"
              "                          GpuIntersectionHitRecord hit) {\n"
              "  const uint mapping = texture.flags & gpuTracingTextureMappingMask;\n"
              "  if (mapping == gpuTracingUvTextureMappingKind) {\n"
              "    return float2(hit.uv.x * texture.parameters.x,\n"
              "                  hit.uv.y * texture.parameters.y);\n"
              "  }\n"
              "  if (mapping == gpuTracingPlanarTextureMappingKind) {\n"
              "    return float2(hit.point.x, hit.point.z);\n"
              "  }\n"
              "  return float2(0.0f);\n"
              "}\n"
              "float normalizedTextureCoordinate(GpuTracingTextureRecord texture,\n"
              "                                  float coordinate) {\n"
              "  if ((texture.flags & gpuTracingTextureWrapClampFlag) != 0u) {\n"
              "    return clamp(coordinate, 0.0f, 1.0f);\n"
              "  }\n"
              "  return coordinate - floor(coordinate);\n"
              "}\n"
              "int imageTextureWrappedCoordinate(GpuTracingTextureRecord texture,\n"
              "                                  int coordinate,\n"
              "                                  int size) {\n"
              "  if ((texture.flags & gpuTracingTextureWrapClampFlag) != 0u) {\n"
              "    return clamp(coordinate, 0, size - 1);\n"
              "  }\n"
              "  int wrapped = coordinate % size;\n"
              "  return wrapped < 0 ? wrapped + size : wrapped;\n"
              "}\n"
              "int imageTextureCoordinate(GpuTracingTextureRecord texture,\n"
              "                           float coordinate,\n"
              "                           int size) {\n"
              "  const int result = int(floor(normalizedTextureCoordinate(texture, coordinate) *\n"
              "                               float(size)));\n"
              "  return imageTextureWrappedCoordinate(texture, result, size);\n"
              "}\n"
              "float4 imageTextureTexelColor(\n"
              "    device const GpuTracingTextureRecord* textures,\n"
              "    uint textureCount,\n"
              "    GpuTracingTextureRecord texture,\n"
              "    int x,\n"
              "    int y,\n"
              "    int width,\n"
              "    thread bool& supported) {\n"
              "  supported = false;\n"
              "  const uint texelTextureId = texture.payloadOffset + uint(y * width + x);\n"
              "  if (texelTextureId >= textureCount) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  const GpuTracingTextureRecord child = textures[texelTextureId];\n"
              "  if (child.kind != gpuTracingConstantColorTextureKind) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  supported = true;\n"
              "  return child.parameters;\n"
              "}\n"
              "float4 untintedTextureColor(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    uint textureId,\n"
              "    GpuIntersectionHitRecord hit,\n"
              "    thread bool& supported) {\n"
              "  supported = false;\n"
              "  if (textureId >= parameters.textureCount) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  device const GpuTracingTextureRecord* textures =\n"
              "      reinterpret_cast<device const GpuTracingTextureRecord*>(\n"
              "          sceneUpload + parameters.textureByteOffset);\n"
              "  const GpuTracingTextureRecord texture = textures[textureId];\n"
              "  if (texture.kind == gpuTracingConstantColorTextureKind) {\n"
              "    supported = true;\n"
              "    return texture.parameters;\n"
              "  }\n"
              "  if (texture.kind == gpuTracingUvColorTextureKind) {\n"
              "    supported = true;\n"
              "    return float4(hit.uv.x, hit.uv.y, 0.0f, 1.0f);\n"
              "  }\n"
              "  if (texture.kind == gpuTracingCheckerBoardTextureKind) {\n"
              "    const uint mapping = texture.flags & gpuTracingTextureMappingMask;\n"
              "    if ((mapping != gpuTracingPlanarTextureMappingKind &&\n"
              "         mapping != gpuTracingUvTextureMappingKind) ||\n"
              "        texture.payloadOffset >= parameters.textureCount ||\n"
              "        texture.payloadCount >= parameters.textureCount) {\n"
              "      return float4(0.0f);\n"
              "    }\n"
              "    const float2 st = textureCoordinates(texture, hit);\n"
              "    const int parity = int(floor(st.x)) + int(floor(st.y));\n"
              "    const uint childTextureId = parity % 2 == 0 ?\n"
              "        texture.payloadOffset : texture.payloadCount;\n"
              "    const GpuTracingTextureRecord child = textures[childTextureId];\n"
              "    if (child.kind != gpuTracingConstantColorTextureKind) {\n"
              "      return float4(0.0f);\n"
              "    }\n"
              "    supported = true;\n"
              "    return child.parameters;\n"
              "  }\n"
              "  if (texture.kind == gpuTracingImageTextureKind) {\n"
              "    const int width = int(round(texture.parameters.z));\n"
              "    const int height = int(round(texture.parameters.w));\n"
              "    if (width <= 0 || height <= 0 ||\n"
              "        texture.payloadOffset >= parameters.textureCount ||\n"
              "        texture.payloadCount != uint(width * height) ||\n"
              "        texture.payloadOffset + texture.payloadCount > parameters.textureCount) {\n"
              "      return float4(0.0f);\n"
              "    }\n"
              "    const float2 st = textureCoordinates(texture, hit);\n"
              "    if ((texture.flags & gpuTracingTextureFilterBilinearFlag) != 0u) {\n"
              "      const float x = normalizedTextureCoordinate(texture, st.x) *\n"
              "          float(width) - 0.5f;\n"
              "      const float y = normalizedTextureCoordinate(texture, st.y) *\n"
              "          float(height) - 0.5f;\n"
              "      const int x0 = int(floor(x));\n"
              "      const int y0 = int(floor(y));\n"
              "      const float tx = x - float(x0);\n"
              "      const float ty = y - float(y0);\n"
              "      bool c00Supported = false;\n"
              "      bool c10Supported = false;\n"
              "      bool c01Supported = false;\n"
              "      bool c11Supported = false;\n"
              "      const float4 c00 = imageTextureTexelColor(\n"
              "          textures, parameters.textureCount, texture,\n"
              "          imageTextureWrappedCoordinate(texture, x0, width),\n"
              "          imageTextureWrappedCoordinate(texture, y0, height), width,\n"
              "          c00Supported);\n"
              "      const float4 c10 = imageTextureTexelColor(\n"
              "          textures, parameters.textureCount, texture,\n"
              "          imageTextureWrappedCoordinate(texture, x0 + 1, width),\n"
              "          imageTextureWrappedCoordinate(texture, y0, height), width,\n"
              "          c10Supported);\n"
              "      const float4 c01 = imageTextureTexelColor(\n"
              "          textures, parameters.textureCount, texture,\n"
              "          imageTextureWrappedCoordinate(texture, x0, width),\n"
              "          imageTextureWrappedCoordinate(texture, y0 + 1, height), width,\n"
              "          c01Supported);\n"
              "      const float4 c11 = imageTextureTexelColor(\n"
              "          textures, parameters.textureCount, texture,\n"
              "          imageTextureWrappedCoordinate(texture, x0 + 1, width),\n"
              "          imageTextureWrappedCoordinate(texture, y0 + 1, height), width,\n"
              "          c11Supported);\n"
              "      if (!c00Supported || !c10Supported || !c01Supported ||\n"
              "          !c11Supported) {\n"
              "        return float4(0.0f);\n"
              "      }\n"
              "      supported = true;\n"
              "      const float4 xBlend0 = c00 * (1.0f - tx) + c10 * tx;\n"
              "      const float4 xBlend1 = c01 * (1.0f - tx) + c11 * tx;\n"
              "      return xBlend0 * (1.0f - ty) + xBlend1 * ty;\n"
              "    }\n"
              "    const int x = imageTextureCoordinate(texture, st.x, width);\n"
              "    const int y = imageTextureCoordinate(texture, st.y, height);\n"
              "    bool texelSupported = false;\n"
              "    const float4 texel = imageTextureTexelColor(\n"
              "        textures, parameters.textureCount, texture, x, y, width,\n"
              "        texelSupported);\n"
              "    if (!texelSupported) {\n"
              "      return float4(0.0f);\n"
              "    }\n"
              "    supported = true;\n"
              "    return texel;\n"
              "  }\n"
              "  return float4(0.0f);\n"
              "}\n"
              "float4 textureColor(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    uint textureId,\n"
              "    GpuIntersectionHitRecord hit,\n"
              "    thread bool& supported) {\n"
              "  supported = false;\n"
              "  float4 tint = float4(1.0f);\n"
              "  uint currentTexture = textureId;\n"
              "  device const GpuTracingTextureRecord* textures =\n"
              "      reinterpret_cast<device const GpuTracingTextureRecord*>(\n"
              "          sceneUpload + parameters.textureByteOffset);\n"
              "  for (uint depth = 0u; depth != pathLoopMaxTextureEvaluationDepth; ++depth) {\n"
              "    if (currentTexture >= parameters.textureCount) {\n"
              "      return float4(0.0f);\n"
              "    }\n"
              "    const GpuTracingTextureRecord texture = textures[currentTexture];\n"
              "    if (texture.kind == gpuTracingTintedTextureKind) {\n"
              "      tint *= texture.parameters;\n"
              "      currentTexture = texture.payloadOffset;\n"
              "      continue;\n"
              "    }\n"
              "    if (texture.kind == gpuTracingCheckerBoardTextureKind) {\n"
              "      const uint mapping = texture.flags & gpuTracingTextureMappingMask;\n"
              "      if ((mapping != gpuTracingPlanarTextureMappingKind &&\n"
              "           mapping != gpuTracingUvTextureMappingKind) ||\n"
              "          texture.payloadOffset >= parameters.textureCount ||\n"
              "          texture.payloadCount >= parameters.textureCount) {\n"
              "        return float4(0.0f);\n"
              "      }\n"
              "      const float2 st = textureCoordinates(texture, hit);\n"
              "      const int parity = int(floor(st.x)) + int(floor(st.y));\n"
              "      currentTexture = parity % 2 == 0 ? texture.payloadOffset :\n"
              "                                           texture.payloadCount;\n"
              "      continue;\n"
              "    }\n"
              "    bool baseSupported = false;\n"
              "    const float4 base = untintedTextureColor(\n"
              "        parameters, sceneUpload, currentTexture, hit, baseSupported);\n"
              "    if (!baseSupported) {\n"
              "      return float4(0.0f);\n"
              "    }\n"
              "    supported = true;\n"
              "    return base * tint;\n"
              "  }\n"
              "  return float4(0.0f);\n"
              "}\n";
    }

    NSString* diffusePathLoopKernelSourceSuffix() {
      return @"GpuTracingMaterialRecord materialRecord(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    uint materialId,\n"
              "    thread bool& supported) {\n"
              "  supported = false;\n"
              "  GpuTracingMaterialRecord material;\n"
              "  material.kind = 0u;\n"
              "  material.albedoTexture = 0u;\n"
              "  material.emissionTexture = 0u;\n"
              "  material.flags = 0u;\n"
              "  material.parameters = float4(0.0f);\n"
              "  material.specularParameters = float4(0.0f);\n"
              "  material.continuationParameters = float4(0.0f);\n"
              "  material.transmissionParameters = float4(0.0f);\n"
              "  if (materialId >= parameters.materialCount) {\n"
              "    return material;\n"
              "  }\n"
              "  device const GpuTracingMaterialRecord* materials =\n"
              "      reinterpret_cast<device const GpuTracingMaterialRecord*>(\n"
              "          sceneUpload + parameters.materialByteOffset);\n"
              "  supported = true;\n"
              "  return materials[materialId];\n"
              "}\n"
              "float4 matteDiffuseReflectance(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    const GpuIntersectionHitRecord hit,\n"
              "    thread bool& supported) {\n"
              "  bool materialSupported = false;\n"
              "  const GpuTracingMaterialRecord material = materialRecord(\n"
              "      parameters, sceneUpload, hit.material, materialSupported);\n"
              "  supported = false;\n"
              "  if (!materialSupported ||\n"
              "      (material.kind != gpuTracingMatteMaterialKind &&\n"
              "       material.kind != gpuTracingPhongMaterialKind &&\n"
              "       material.kind != gpuTracingReflectiveMaterialKind &&\n"
              "       material.kind != gpuTracingTransparentMaterialKind)) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  bool textureSupported = false;\n"
              "  const float4 albedo = textureColor(\n"
              "      parameters, sceneUpload, material.albedoTexture, hit, textureSupported);\n"
              "  if (!textureSupported) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  supported = true;\n"
              "  return albedo * material.parameters.y;\n"
              "}\n"
              "float powerHeuristic(float sampledPdf, float otherPdf) {\n"
              "  const float sampled = max(0.0f, sampledPdf);\n"
              "  const float other = max(0.0f, otherPdf);\n"
              "  const float sampledSquared = sampled * sampled;\n"
              "  const float otherSquared = other * other;\n"
              "  const float denominator = sampledSquared + otherSquared;\n"
              "  return denominator == 0.0f ? 0.0f : sampledSquared / denominator;\n"
              "}\n"
              "float emitterHitMisWeight(GpuDiffusePathStateRecord path) {\n"
              "  const bool sampledFromBsdf =\n"
              "      (path.previousEventFlags & gpuDiffusePathStateSampledFromBsdfFlag) != 0u;\n"
              "  const bool sampledDelta =\n"
              "      (path.previousEventFlags & gpuDiffusePathStateBsdfSampleDeltaFlag) != 0u;\n"
              "  if (!sampledFromBsdf || sampledDelta) {\n"
              "    return 1.0f;\n"
              "  }\n"
              "  return powerHeuristic(path.previousBsdfPdf, path.previousLightPdf);\n"
              "}\n"
              "float4 emissiveContribution(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    const GpuIntersectionHitRecord hit,\n"
              "    const GpuDiffusePathStateRecord path,\n"
              "    thread bool& supported) {\n"
              "  bool materialSupported = false;\n"
              "  const GpuTracingMaterialRecord material = materialRecord(\n"
              "      parameters, sceneUpload, hit.material, materialSupported);\n"
              "  supported = false;\n"
              "  if (!materialSupported || material.kind != gpuTracingEmissiveMaterialKind) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  bool textureSupported = false;\n"
              "  const float4 emitted = textureColor(\n"
              "      parameters, sceneUpload, material.emissionTexture, hit, textureSupported);\n"
              "  if (!textureSupported) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  supported = true;\n"
              "  const float3 normal = normalize(hit.normal.xyz);\n"
              "  const float3 incoming = -normalize(path.ray.direction.xyz);\n"
              "  return dot(normal, incoming) > 0.0f ?\n"
              "      path.throughput * emitted * emitterHitMisWeight(path) : float4(0.0f);\n"
              "}\n"
              "GpuDiffusePathStateRecord terminatedPathWithAccumulatedRadiance(\n"
              "    const GpuDiffusePathStateRecord path,\n"
              "    float4 accumulatedRadiance) {\n"
              "  GpuDiffusePathStateRecord next = path;\n"
              "  next.accumulatedRadiance = accumulatedRadiance;\n"
              "  next.flags = terminatedPathFlags(path.flags);\n"
              "  return next;\n"
              "}\n"
              "float4 matteContinuationThroughput(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    const GpuIntersectionHitRecord hit,\n"
              "    const GpuDiffusePathStateRecord path,\n"
              "    thread bool& supported) {\n"
              "  const float4 reflectance = matteDiffuseReflectance(\n"
              "      parameters, sceneUpload, hit, supported);\n"
              "  return path.throughput * reflectance;\n"
              "}\n"
              "float4 mirrorReflectance(GpuTracingMaterialRecord material) {\n"
              "  return float4(material.continuationParameters.xyz *\n"
              "                material.continuationParameters.w, 0.0f);\n"
              "}\n"
              "float diffuseSamplingWeight(GpuTracingMaterialRecord material) {\n"
              "  const float diffuse = max(0.0f, material.parameters.y);\n"
              "  const float specular = max(0.0f, material.parameters.z);\n"
              "  const float total = diffuse + specular;\n"
              "  return total <= 0.0f ? 1.0f : diffuse / total;\n"
              "}\n"
              "float4 glossyPhongBsdf(GpuTracingMaterialRecord material, float3 normal,\n"
              "                       float3 wi, float3 wo) {\n"
              "  if (dot(normal, wi) < 0.0f || dot(normal, wo) < 0.0f) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  const float3 lobeAxis = normalize(reflect(-wi, normal));\n"
              "  const float lobeDotOut = dot(lobeAxis, normalize(wo));\n"
              "  if (lobeDotOut <= 0.0f) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  return float4(material.specularParameters.xyz *\n"
              "                material.parameters.z * pow(lobeDotOut, material.parameters.w), 0.0f);\n"
              "}\n"
              "bool hasFinitePhongLobes(GpuTracingMaterialRecord material) {\n"
              "  return material.kind == gpuTracingPhongMaterialKind ||\n"
              "         material.kind == gpuTracingReflectiveMaterialKind ||\n"
              "         material.kind == gpuTracingTransparentMaterialKind;\n"
              "}\n"
              "float4 finiteBsdf(float4 matteReflectance, GpuTracingMaterialRecord material,\n"
              "                  float3 normal, float3 wi, float3 wo) {\n"
              "  float4 value = matteReflectance * pathLoopInvPi;\n"
              "  if (hasFinitePhongLobes(material)) {\n"
              "    value += glossyPhongBsdf(material, normal, wi, wo);\n"
              "  }\n"
              "  return value;\n"
              "}\n"
              "float phongLobePdf(GpuTracingMaterialRecord material, float3 normal,\n"
              "                   float3 wi, float3 wo) {\n"
              "  if (dot(normal, wi) < 0.0f || dot(normal, wo) < 0.0f) {\n"
              "    return 0.0f;\n"
              "  }\n"
              "  const float3 lobeAxis = normalize(reflect(-wi, normal));\n"
              "  const float lobeDotOut = dot(lobeAxis, normalize(wo));\n"
              "  if (lobeDotOut <= 0.0f) {\n"
              "    return 0.0f;\n"
              "  }\n"
              "  return ((material.parameters.w + 1.0f) / pathLoopTau) *\n"
              "         pow(lobeDotOut, material.parameters.w);\n"
              "}\n"
              "float finiteBsdfPdf(GpuTracingMaterialRecord material, float3 normal,\n"
              "                    float3 wi, float3 wo) {\n"
              "  if (material.kind == gpuTracingMatteMaterialKind) {\n"
              "    return cosineHemispherePdf(normal, wo);\n"
              "  }\n"
              "  if (material.kind != gpuTracingPhongMaterialKind) {\n"
              "    return 0.0f;\n"
              "  }\n"
              "  const float diffuseWeight = diffuseSamplingWeight(material);\n"
              "  return diffuseWeight * cosineHemispherePdf(normal, wo) +\n"
              "         (1.0f - diffuseWeight) * phongLobePdf(material, normal, wi, wo);\n"
              "}\n"
              "float3 sampleFiniteBsdfDirection(GpuTracingMaterialRecord material,\n"
              "                                 float3 normal, float3 wi, float2 sample) {\n"
              "  if (material.kind != gpuTracingPhongMaterialKind) {\n"
              "    return cosineHemisphereDirection(normal, sample);\n"
              "  }\n"
              "  const float diffuseWeight = diffuseSamplingWeight(material);\n"
              "  const float selector = clamp(sample.x, 0.0f, 1.0f);\n"
              "  const float y = clamp(sample.y, 0.0f, 1.0f);\n"
              "  if (diffuseWeight >= 1.0f || selector < diffuseWeight) {\n"
              "    const float remappedX = diffuseWeight > 0.0f ? selector / diffuseWeight : selector;\n"
              "    return cosineHemisphereDirection(normal, float2(remappedX, y));\n"
              "  }\n"
              "  const float specularWeight = 1.0f - diffuseWeight;\n"
              "  const float remappedX = specularWeight > 0.0f ?\n"
              "      (selector - diffuseWeight) / specularWeight : selector;\n"
              "  return phongLobeDirection(normalize(reflect(-wi, normal)), float2(remappedX, y),\n"
              "                            material.parameters.w);\n"
              "}\n"
              "float rectangularLightArea(GpuTracingLightRecord light) {\n"
              "  return length(cross(light.u.xyz, light.v.xyz));\n"
              "}\n"
              "float compiledLightSelectionWeight(GpuTracingLightRecord light) {\n"
              "  if (light.kind == gpuTracingPointLightKind ||\n"
              "      light.kind == gpuTracingDirectionalLightKind) {\n"
              "    return maxColor(light.parameters);\n"
              "  }\n"
              "  if (light.kind == gpuTracingRectangularAreaLightKind) {\n"
              "    return maxColor(light.parameters) * rectangularLightArea(light) *\n"
              "           (pathLoopTau * 0.5f);\n"
              "  }\n"
              "  return 0.0f;\n"
              "}\n"
              "DirectLightSelection selectDirectLight(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    const GpuDiffusePathStateRecord path,\n"
              "    uint directSampleIndex) {\n"
              "  DirectLightSelection selection;\n"
              "  selection.valid = 0u;\n"
              "  selection.lightIndex = 0u;\n"
              "  selection.pdf = 0.0f;\n"
              "  if (parameters.lightCount == 0u) {\n"
              "    return selection;\n"
              "  }\n"
              "  device const GpuTracingLightRecord* lights =\n"
              "      reinterpret_cast<device const GpuTracingLightRecord*>(\n"
              "          sceneUpload + parameters.lightByteOffset);\n"
              "  float totalWeight = 0.0f;\n"
              "  for (uint lightIndex = 0u; lightIndex != parameters.lightCount; ++lightIndex) {\n"
              "    totalWeight += compiledLightSelectionWeight(lights[lightIndex]);\n"
              "  }\n"
              "  const bool useUniformWeights = totalWeight <= 0.0f;\n"
              "  if (useUniformWeights) {\n"
              "    totalWeight = float(parameters.lightCount);\n"
              "  }\n"
              "  const float unitSample = clamp(\n"
              "      sample1D(path, lightSelectionDimension(path, directSampleIndex), 0u),\n"
              "      0.0f, 0.9999999403953552f);\n"
              "  const float target = unitSample * totalWeight;\n"
              "  float cumulative = 0.0f;\n"
              "  for (uint lightIndex = 0u; lightIndex != parameters.lightCount; ++lightIndex) {\n"
              "    const float weight = useUniformWeights ? 1.0f :\n"
              "        compiledLightSelectionWeight(lights[lightIndex]);\n"
              "    cumulative += weight;\n"
              "    if (target < cumulative) {\n"
              "      selection.valid = 1u;\n"
              "      selection.lightIndex = lightIndex;\n"
              "      selection.pdf = weight / totalWeight;\n"
              "      return selection;\n"
              "    }\n"
              "  }\n"
              "  return selection;\n"
              "}\n"
              "float rectangularLightPdf(GpuTracingLightRecord light, float3 point,\n"
              "                          float3 direction) {\n"
              "  const float area = rectangularLightArea(light);\n"
              "  if (area <= pathLoopRayEpsilon) {\n"
              "    return 0.0f;\n"
              "  }\n"
              "  const float3 lightNormal = cross(light.u.xyz, light.v.xyz) / area;\n"
              "  const float normalDotDirection = dot(lightNormal, direction);\n"
              "  if (abs(normalDotDirection) <= pathLoopRayEpsilon) {\n"
              "    return 0.0f;\n"
              "  }\n"
              "  const float distance =\n"
              "      dot(light.positionOrDirection.xyz - point, lightNormal) / normalDotDirection;\n"
              "  if (distance <= pathLoopRayEpsilon) {\n"
              "    return 0.0f;\n"
              "  }\n"
              "  const float3 lightPoint = point + direction * distance;\n"
              "  const float3 local = lightPoint - light.positionOrDirection.xyz;\n"
              "  const float uu = dot(light.u.xyz, light.u.xyz);\n"
              "  const float uv = dot(light.u.xyz, light.v.xyz);\n"
              "  const float vv = dot(light.v.xyz, light.v.xyz);\n"
              "  const float lu = dot(local, light.u.xyz);\n"
              "  const float lv = dot(local, light.v.xyz);\n"
              "  const float determinant = uu * vv - uv * uv;\n"
              "  if (abs(determinant) <= pathLoopRayEpsilon) {\n"
              "    return 0.0f;\n"
              "  }\n"
              "  const float localU = (vv * lu - uv * lv) / determinant;\n"
              "  const float localV = (uu * lv - uv * lu) / determinant;\n"
              "  if (localU < -0.5f - pathLoopRayEpsilon ||\n"
              "      localU > 0.5f + pathLoopRayEpsilon ||\n"
              "      localV < -0.5f - pathLoopRayEpsilon ||\n"
              "      localV > 0.5f + pathLoopRayEpsilon) {\n"
              "    return 0.0f;\n"
              "  }\n"
              "  const float lightCosine = max(0.0f, dot(lightNormal, -normalize(direction)));\n"
              "  if (lightCosine <= pathLoopRayEpsilon) {\n"
              "    return 0.0f;\n"
              "  }\n"
              "  return (distance * distance) / (lightCosine * area);\n"
              "}\n"
              "float lightPdf(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    float3 point,\n"
              "    float3 direction) {\n"
              "  if (parameters.lightCount == 0u) {\n"
              "    return 0.0f;\n"
              "  }\n"
              "  device const GpuTracingLightRecord* lights =\n"
              "      reinterpret_cast<device const GpuTracingLightRecord*>(\n"
              "          sceneUpload + parameters.lightByteOffset);\n"
              "  float totalWeight = 0.0f;\n"
              "  for (uint lightIndex = 0u; lightIndex != parameters.lightCount; ++lightIndex) {\n"
              "    totalWeight += compiledLightSelectionWeight(lights[lightIndex]);\n"
              "  }\n"
              "  const bool useUniformWeights = totalWeight <= 0.0f;\n"
              "  if (useUniformWeights) {\n"
              "    totalWeight = float(parameters.lightCount);\n"
              "  }\n"
              "  float pdf = 0.0f;\n"
              "  for (uint lightIndex = 0u; lightIndex != parameters.lightCount; ++lightIndex) {\n"
              "    const GpuTracingLightRecord light = lights[lightIndex];\n"
              "    const float selectionWeight = useUniformWeights ? 1.0f :\n"
              "        compiledLightSelectionWeight(light);\n"
              "    if (selectionWeight <= 0.0f) {\n"
              "      continue;\n"
              "    }\n"
              "    if (light.kind == gpuTracingRectangularAreaLightKind) {\n"
              "      pdf += (selectionWeight / totalWeight) *\n"
              "          rectangularLightPdf(light, point, direction);\n"
              "    }\n"
              "  }\n"
              "  return pdf;\n"
              "}\n"
              "DirectLightSample sampleDirectLight(GpuTracingLightRecord light, float3 point,\n"
              "                                    float2 surfaceSample) {\n"
              "  DirectLightSample sample;\n"
              "  sample.valid = 0u;\n"
              "  sample.delta = 0u;\n"
              "  sample.direction = float3(0.0f);\n"
              "  sample.radiance = float4(0.0f);\n"
              "  sample.distance = 0.0f;\n"
              "  sample.pdf = 0.0f;\n"
              "  if (light.kind == gpuTracingPointLightKind) {\n"
              "    const float3 offset = light.positionOrDirection.xyz - point;\n"
              "    const float distance = length(offset);\n"
              "    if (distance <= 1.0e-7f) {\n"
              "      return sample;\n"
              "    }\n"
              "    sample.valid = 1u;\n"
              "    sample.delta = 1u;\n"
              "    sample.direction = offset / distance;\n"
              "    sample.radiance = light.parameters;\n"
              "    sample.distance = distance;\n"
              "    sample.pdf = 1.0f;\n"
              "    return sample;\n"
              "  }\n"
              "  if (light.kind == gpuTracingDirectionalLightKind) {\n"
              "    const float lengthDirection = length(light.positionOrDirection.xyz);\n"
              "    if (lengthDirection <= 1.0e-7f) {\n"
              "      return sample;\n"
              "    }\n"
              "    sample.valid = 1u;\n"
              "    sample.delta = 1u;\n"
              "    sample.direction = light.positionOrDirection.xyz / lengthDirection;\n"
              "    sample.radiance = light.parameters;\n"
              "    sample.distance = rayInfinity();\n"
              "    sample.pdf = 1.0f;\n"
              "    return sample;\n"
              "  }\n"
              "  if (light.kind == gpuTracingRectangularAreaLightKind) {\n"
              "    const float3 crossEdges = cross(light.u.xyz, light.v.xyz);\n"
              "    const float area = length(crossEdges);\n"
              "    if (area <= 1.0e-7f) {\n"
              "      return sample;\n"
              "    }\n"
              "    const float3 lightNormal = crossEdges / area;\n"
              "    const float3 lightPoint = light.positionOrDirection.xyz +\n"
              "        light.u.xyz * (surfaceSample.x - 0.5f) +\n"
              "        light.v.xyz * (surfaceSample.y - 0.5f);\n"
              "    const float3 offset = lightPoint - point;\n"
              "    const float distance = length(offset);\n"
              "    if (distance <= 1.0e-7f) {\n"
              "      return sample;\n"
              "    }\n"
              "    const float3 direction = offset / distance;\n"
              "    const float lightCosine = max(0.0f, dot(lightNormal, -direction));\n"
              "    if (lightCosine <= 1.0e-7f) {\n"
              "      return sample;\n"
              "    }\n"
              "    sample.valid = 1u;\n"
              "    sample.delta = 0u;\n"
              "    sample.direction = direction;\n"
              "    sample.radiance = light.parameters;\n"
              "    sample.distance = distance;\n"
              "    sample.pdf = (distance * distance) / (lightCosine * area);\n"
              "    return sample;\n"
              "  }\n"
              "  return sample;\n"
              "}\n"
              "GpuIntersectionRay shadowRayFor(float3 point, DirectLightSample sample) {\n"
              "  GpuIntersectionRay ray;\n"
              "  ray.origin = float4(point + sample.direction * pathLoopRayEpsilon, 1.0f);\n"
              "  ray.direction = float4(sample.direction, 0.0f);\n"
              "  ray.minDistance = pathLoopMinimumHitDistance;\n"
              "  ray.maxDistance = sample.distance;\n"
              "  ray.timeSample = 0.0f;\n"
              "  ray.flags = 0u;\n"
              "  ray.rayIndex = 0u;\n"
              "  ray.reserved0 = 0u;\n"
              "  ray.reserved1 = 0u;\n"
              "  ray.reserved2 = 0u;\n"
              "  return ray;\n"
              "}\n"
              "bool hitOccludesLight(GpuIntersectionHitRecord hit, float lightDistance) {\n"
              "  if (hit.hit == 0u) {\n"
              "    return false;\n"
              "  }\n"
              "  if (isinf(lightDistance)) {\n"
              "    return true;\n"
              "  }\n"
              "  const float endpointTolerance = max(pathLoopMinimumHitDistance,\n"
              "                                      lightDistance * 1.0e-5f);\n"
              "  const float occlusionLimit = max(0.0f, lightDistance - endpointTolerance);\n"
              "  return hit.distance < occlusionLimit;\n"
              "}\n"
              "float4 directLightRadiance(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    const GpuIntersectionHitRecord hit,\n"
              "    const GpuDiffusePathStateRecord path,\n"
              "    GpuTracingMaterialRecord material,\n"
              "    float4 matteReflectance) {\n"
              "  if (parameters.lightCount == 0u) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  const uint sampleCount = max(parameters.directLightSamples, 1u);\n"
              "  device const GpuTracingLightRecord* lights =\n"
              "      reinterpret_cast<device const GpuTracingLightRecord*>(\n"
              "          sceneUpload + parameters.lightByteOffset);\n"
              "  const float3 point = hit.point.xyz;\n"
              "  const float3 normal = normalize(hit.normal.xyz);\n"
              "  const float3 wi = -normalize(path.ray.direction.xyz);\n"
              "  float4 radiance = float4(0.0f);\n"
              "  for (uint sampleIndex = 0u; sampleIndex != sampleCount; ++sampleIndex) {\n"
              "    const DirectLightSelection selection = selectDirectLight(\n"
              "        parameters, sceneUpload, path, sampleIndex);\n"
              "    if (selection.valid == 0u || selection.lightIndex >= parameters.lightCount ||\n"
              "        selection.pdf <= 0.0f) {\n"
              "      continue;\n"
              "    }\n"
              "    const float2 lightSample = sample2D(\n"
              "        path, lightSurfaceDimension(path, selection.lightIndex, sampleIndex));\n"
              "    const DirectLightSample light = sampleDirectLight(\n"
              "        lights[selection.lightIndex], point, lightSample);\n"
              "    const float normalDotLight = dot(normal, light.direction);\n"
              "    if (light.valid == 0u || light.pdf <= 0.0f || normalDotLight <= 0.0f) {\n"
              "      continue;\n"
              "    }\n"
              "    const GpuIntersectionRay visibilityRay = shadowRayFor(point, light);\n"
              "    const GpuIntersectionHitRecord visibilityHit =\n"
              "        closestSupportedHit(parameters, sceneUpload, visibilityRay);\n"
              "    if (hitOccludesLight(visibilityHit, light.distance)) {\n"
              "      continue;\n"
              "    }\n"
              "    const float4 bsdfValue = finiteBsdf(matteReflectance, material, normal,\n"
              "                                        wi, light.direction);\n"
              "    const float bsdfPdf = light.delta != 0u ? 0.0f :\n"
              "        finiteBsdfPdf(material, normal, wi, light.direction);\n"
              "    const float misWeight = light.delta != 0u ? 1.0f :\n"
              "        (light.pdf * light.pdf) /\n"
              "        (light.pdf * light.pdf + bsdfPdf * bsdfPdf);\n"
              "    const float4 contribution = path.throughput * bsdfValue * light.radiance *\n"
              "                                (misWeight * normalDotLight /\n"
              "                                 (light.pdf * selection.pdf));\n"
              "    radiance += contribution;\n"
              "  }\n"
              "  return radiance / float(sampleCount);\n"
              "}\n"
              "GpuIntersectionRay continuationRay(const GpuIntersectionHitRecord hit,\n"
              "                                  const GpuDiffusePathStateRecord path,\n"
              "                                  float3 direction) {\n"
              "  GpuIntersectionRay ray = path.ray;\n"
              "  ray.origin = float4(hit.point.xyz + direction * pathLoopRayEpsilon, 1.0f);\n"
              "  ray.direction = float4(direction, 0.0f);\n"
              "  ray.minDistance = pathLoopMinimumHitDistance;\n"
              "  ray.maxDistance = rayInfinity();\n"
              "  ray.timeSample = 0.0f;\n"
              "  return ray;\n"
              "}\n"
              "GpuDiffusePathStateRecord matteContinuationPath(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    const GpuIntersectionHitRecord hit,\n"
              "    const GpuDiffusePathStateRecord path,\n"
              "    GpuTracingMaterialRecord material,\n"
              "    float4 matteReflectance,\n"
              "    float4 accumulatedRadiance,\n"
              "    thread bool& spawned) {\n"
              "  spawned = false;\n"
              "  GpuDiffusePathStateRecord next = path;\n"
              "  next.accumulatedRadiance = accumulatedRadiance;\n"
              "  const float3 normal = normalize(hit.normal.xyz);\n"
              "  const float3 wi = -normalize(path.ray.direction.xyz);\n"
              "  const float2 bsdfSample = sample2D(path, sampleDimension(path, 0u));\n"
              "  const float3 direction = sampleFiniteBsdfDirection(material, normal, wi, bsdfSample);\n"
              "  const float pdf = finiteBsdfPdf(material, normal, wi, direction);\n"
              "  const float normalDotOut = dot(normal, direction);\n"
              "  float4 continuationThroughput = float4(0.0f);\n"
              "  if (pdf > 0.0f && normalDotOut > 0.0f) {\n"
              "    continuationThroughput = path.throughput *\n"
              "        finiteBsdf(matteReflectance, material, normal, wi, direction) *\n"
              "        (normalDotOut / pdf);\n"
              "  }\n"
              "  if (path.depth >= parameters.russianRouletteDepth) {\n"
              "    const float probability = continuationProbability(continuationThroughput);\n"
              "    const float roulette = sample1D(path, sampleDimension(path, 3u), 0u);\n"
              "    continuationThroughput = roulette < probability && probability > 0.0f ?\n"
              "      continuationThroughput * (1.0f / probability) : float4(0.0f);\n"
              "  }\n"
              "  if (throughputIsBlack(continuationThroughput)) {\n"
              "    next.flags = terminatedPathFlags(path.flags);\n"
              "    next.throughput = float4(0.0f);\n"
              "    return next;\n"
              "  }\n"
              "  next.ray = continuationRay(hit, path, direction);\n"
              "  next.throughput = continuationThroughput;\n"
              "  next.depth = path.depth + 1u;\n"
              "  next.previousBsdfPdf = pdf;\n"
              "  next.previousLightPdf = lightPdf(parameters, sceneUpload, hit.point.xyz,\n"
              "                                    direction);\n"
              "  next.previousMaterial = hit.material;\n"
              "  next.previousEventFlags = gpuDiffusePathStateSampledFromBsdfFlag;\n"
              "  next.flags = activePathFlags(path.flags);\n"
              "  spawned = true;\n"
              "  return next;\n"
              "}\n"
              "GpuDiffusePathStateRecord reflectiveContinuationPath(\n"
              "    const GpuIntersectionHitRecord hit,\n"
              "    const GpuDiffusePathStateRecord path,\n"
              "    float4 continuationThroughput,\n"
              "    float4 accumulatedRadiance,\n"
              "    thread bool& spawned) {\n"
              "  spawned = false;\n"
              "  GpuDiffusePathStateRecord next = path;\n"
              "  next.accumulatedRadiance = accumulatedRadiance;\n"
              "  if (throughputIsBlack(continuationThroughput)) {\n"
              "    next.flags = terminatedPathFlags(path.flags);\n"
              "    next.throughput = float4(0.0f);\n"
              "    return next;\n"
              "  }\n"
              "  const float3 normal = normalize(hit.normal.xyz);\n"
              "  const float3 direction = normalize(reflect(normalize(path.ray.direction.xyz),\n"
              "                                             normal));\n"
              "  next.ray = continuationRay(hit, path, direction);\n"
              "  next.throughput = continuationThroughput;\n"
              "  next.depth = path.depth + 1u;\n"
              "  next.previousBsdfPdf = 1.0f;\n"
              "  next.previousLightPdf = 0.0f;\n"
              "  next.previousMaterial = hit.material;\n"
              "  next.previousEventFlags = gpuDiffusePathStateSampledFromBsdfFlag |\n"
              "                            gpuDiffusePathStateBsdfSampleDeltaFlag;\n"
              "  next.flags = activePathFlags(path.flags);\n"
              "  spawned = true;\n"
              "  return next;\n"
              "}\n"
              "float transparentEta(GpuTracingMaterialRecord material, float3 wi,\n"
              "                     float3 normal) {\n"
              "  const float ior = material.transmissionParameters.y;\n"
              "  return dot(normal, wi) < 0.0f ? 1.0f / ior : ior;\n"
              "}\n"
              "bool transparentTotalInternalReflection(GpuTracingMaterialRecord material,\n"
              "                                        float3 wi, float3 normal) {\n"
              "  const float cosTheta = dot(normal, wi);\n"
              "  const float eta = transparentEta(material, wi, normal);\n"
              "  return 1.0f - (1.0f - cosTheta * cosTheta) / (eta * eta) < 0.0f;\n"
              "}\n"
              "float3 transparentRefract(float3 incident, float3 normal, float eta) {\n"
              "  const float cosTheta = dot(incident, normal);\n"
              "  const float cosTheta2 = sqrt(max(0.0f,\n"
              "      1.0f - (1.0f - cosTheta * cosTheta) / (eta * eta)));\n"
              "  return normalize(-(incident / eta) - normal * (cosTheta2 - cosTheta / eta));\n"
              "}\n"
              "float3 transparentTransmissionDirection(GpuTracingMaterialRecord material,\n"
              "                                      float3 wi, float3 normal) {\n"
              "  float3 orientedNormal = normal;\n"
              "  float eta = material.transmissionParameters.y;\n"
              "  if (dot(orientedNormal, wi) < 0.0f) {\n"
              "    orientedNormal = -orientedNormal;\n"
              "    eta = 1.0f / eta;\n"
              "  }\n"
              "  return transparentRefract(wi, orientedNormal, eta);\n"
              "}\n"
              "GpuDiffusePathStateRecord transparentContinuationPath(\n"
              "    const GpuIntersectionHitRecord hit,\n"
              "    const GpuDiffusePathStateRecord path,\n"
              "    GpuTracingMaterialRecord material,\n"
              "    float4 accumulatedRadiance,\n"
              "    thread bool& spawned) {\n"
              "  spawned = false;\n"
              "  GpuDiffusePathStateRecord next = path;\n"
              "  next.accumulatedRadiance = accumulatedRadiance;\n"
              "  const float3 normal = normalize(hit.normal.xyz);\n"
              "  const float3 wi = -normalize(path.ray.direction.xyz);\n"
              "  float3 direction = normalize(reflect(normalize(path.ray.direction.xyz), normal));\n"
              "  float4 branchWeight = float4(1.0f);\n"
              "  if (!transparentTotalInternalReflection(material, wi, normal)) {\n"
              "    const float reflection = max(0.0f, material.continuationParameters.w);\n"
              "    const float transmission = max(0.0f, material.transmissionParameters.x);\n"
              "    const float total = reflection + transmission;\n"
              "    if (total <= 0.0f) {\n"
              "      next.flags = terminatedPathFlags(path.flags);\n"
              "      next.throughput = float4(0.0f);\n"
              "      return next;\n"
              "    }\n"
              "    const float reflectionWeight = reflection / total;\n"
              "    const float selector = clamp(sample2D(path, sampleDimension(path, 0u)).x,\n"
              "                                 0.0f, 1.0f);\n"
              "    if (reflectionWeight > 0.0f && selector < reflectionWeight) {\n"
              "      branchWeight = mirrorReflectance(material) * (1.0f / reflectionWeight);\n"
              "    } else {\n"
              "      const float transmissionWeight = 1.0f - reflectionWeight;\n"
              "      if (transmissionWeight <= 0.0f) {\n"
              "        next.flags = terminatedPathFlags(path.flags);\n"
              "        next.throughput = float4(0.0f);\n"
              "        return next;\n"
              "      }\n"
              "      const float eta = transparentEta(material, wi, normal);\n"
              "      direction = transparentTransmissionDirection(material, wi, normal);\n"
              "      branchWeight = float4(material.transmissionParameters.x /\n"
              "                            (eta * eta * transmissionWeight));\n"
              "    }\n"
              "  }\n"
              "  const float4 continuationThroughput = path.throughput * branchWeight;\n"
              "  if (throughputIsBlack(continuationThroughput)) {\n"
              "    next.flags = terminatedPathFlags(path.flags);\n"
              "    next.throughput = float4(0.0f);\n"
              "    return next;\n"
              "  }\n"
              "  next.ray = continuationRay(hit, path, direction);\n"
              "  next.throughput = continuationThroughput;\n"
              "  next.depth = path.depth + 1u;\n"
              "  next.previousBsdfPdf = 1.0f;\n"
              "  next.previousLightPdf = 0.0f;\n"
              "  next.previousMaterial = hit.material;\n"
              "  next.previousEventFlags = gpuDiffusePathStateSampledFromBsdfFlag |\n"
              "                            gpuDiffusePathStateBsdfSampleDeltaFlag;\n"
              "  next.flags = activePathFlags(path.flags);\n"
              "  spawned = true;\n"
              "  return next;\n"
              "}\n"
              "uint unsupportedPathFlags(uint flags) {\n"
              "  return (flags | gpuDiffusePathStateUnsupportedFlag |\n"
              "          gpuDiffusePathStateTerminatedFlag) & ~gpuDiffusePathStateActiveFlag;\n"
              "}\n"
              "GpuDiffusePathStepRecord inactiveStep(uint pathIndex,\n"
              "                                      GpuDiffusePathStateRecord path) {\n"
              "  GpuDiffusePathStepRecord step;\n"
              "  step.event = gpuDiffusePathStepEventInactive;\n"
              "  step.pathIndex = pathIndex;\n"
              "  step.pixelIndex = path.pixelIndex;\n"
              "  step.primarySampleIndex = path.primarySampleIndex;\n"
              "  step.depth = path.depth;\n"
              "  step.material = 0u;\n"
              "  step.object = 0u;\n"
              "  step.flags = path.flags;\n"
              "  step.emittedRadiance = float4(0.0f);\n"
              "  step.directLightRadiance = float4(0.0f);\n"
              "  step.missRadiance = float4(0.0f);\n"
              "  step.continuationThroughput = path.throughput;\n"
              "  return step;\n"
              "}\n"
              "bool usesAmbientEnvironmentLayout(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters) {\n"
              "  return parameters.environmentCount >= 3u;\n"
              "}\n"
              "float4 environmentColor(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    uint environmentIndex) {\n"
              "  if (parameters.environmentCount == 0u) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  if (environmentIndex >= parameters.environmentCount) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  device const GpuTracingEnvironmentRecord* environment =\n"
              "      reinterpret_cast<device const GpuTracingEnvironmentRecord*>(\n"
              "          sceneUpload + parameters.environmentByteOffset);\n"
              "  return environment[environmentIndex].color;\n"
              "}\n"
              "float4 sceneAmbientRadiance(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload) {\n"
              "  return usesAmbientEnvironmentLayout(parameters)\n"
              "      ? environmentColor(parameters, sceneUpload, 0u)\n"
              "      : float4(0.0f);\n"
              "}\n"
              "float4 missRadiance(constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "                    device const uchar* sceneUpload,\n"
              "                    GpuDiffusePathStateRecord path) {\n"
              "  if (parameters.environmentCount == 0u) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  const uint environmentIndex = path.depth == 0u\n"
              "      ? (usesAmbientEnvironmentLayout(parameters) ? 1u : 0u)\n"
              "      : parameters.environmentCount - 1u;\n"
              "  return environmentColor(parameters, sceneUpload, environmentIndex);\n"
              "}\n"
              "float4 ambientRadiance(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    GpuTracingMaterialRecord material,\n"
              "    const GpuIntersectionHitRecord hit,\n"
              "    thread bool& supported) {\n"
              "  bool textureSupported = false;\n"
              "  const float4 albedo = textureColor(\n"
              "      parameters, sceneUpload, material.albedoTexture, hit, textureSupported);\n"
              "  supported = textureSupported;\n"
              "  return textureSupported\n"
              "      ? albedo * material.parameters.x * sceneAmbientRadiance(parameters, sceneUpload)\n"
              "      : float4(0.0f);\n"
              "}\n"
              "GpuDiffusePathStepRecord mattePathStep(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    uint pathIndex,\n"
              "    GpuDiffusePathStateRecord path,\n"
              "    thread GpuDiffusePathStateRecord& next,\n"
              "    thread GpuIntersectionHitRecord& hit) {\n"
              "  next = path;\n"
              "  GpuDiffusePathStepRecord step = inactiveStep(pathIndex, path);\n"
              "  hit = missHitRecord(path.ray);\n"
              "  if (pathStateIsActive(path)) {\n"
              "    hit = closestSupportedHit(parameters, sceneUpload, path.ray);\n"
              "    if (hit.hit != 0u) {\n"
              "      bool materialSupported = false;\n"
              "      const GpuTracingMaterialRecord material = materialRecord(\n"
              "          parameters, sceneUpload, hit.material, materialSupported);\n"
              "      bool matteSupported = false;\n"
              "      const float4 reflectance = matteDiffuseReflectance(\n"
              "          parameters, sceneUpload, hit, matteSupported);\n"
              "      step.event = gpuDiffusePathStepEventHit;\n"
              "      step.material = hit.material;\n"
              "      step.object = hit.object;\n"
              "      if (matteSupported) {\n"
              "        bool ambientSupported = false;\n"
              "        const float4 ambient = path.throughput * ambientRadiance(\n"
              "            parameters, sceneUpload, material, hit, ambientSupported);\n"
              "        if (!ambientSupported) {\n"
              "          step.event = gpuDiffusePathStepEventUnsupported;\n"
              "          next.flags = unsupportedPathFlags(path.flags);\n"
              "          step.continuationThroughput = float4(0.0f);\n"
              "          step.flags = next.flags;\n"
              "          return step;\n"
              "        }\n"
              "        const float4 directLight = directLightRadiance(\n"
              "            parameters, sceneUpload, hit, path, material, reflectance);\n"
              "        const float4 accumulatedRadiance =\n"
              "            path.accumulatedRadiance + ambient + directLight;\n"
              "        step.directLightRadiance = directLight;\n"
              "        bool spawned = false;\n"
              "        if (materialSupported &&\n"
              "            material.kind == gpuTracingReflectiveMaterialKind) {\n"
              "          next = reflectiveContinuationPath(\n"
              "              hit, path, path.throughput * mirrorReflectance(material),\n"
              "              accumulatedRadiance, spawned);\n"
              "        } else if (materialSupported &&\n"
              "                   material.kind == gpuTracingTransparentMaterialKind) {\n"
              "          next = transparentContinuationPath(\n"
              "              hit, path, material, accumulatedRadiance, spawned);\n"
              "        } else {\n"
              "          next = matteContinuationPath(parameters, hit, path, material, reflectance,\n"
              "                                       accumulatedRadiance, spawned);\n"
              "        }\n"
              "        step.continuationThroughput = spawned ? next.throughput : float4(0.0f);\n"
              "        step.flags = next.flags;\n"
              "      } else {\n"
              "        bool emissiveSupported = false;\n"
              "        const float4 emitted = emissiveContribution(\n"
              "            parameters, sceneUpload, hit, path, emissiveSupported);\n"
              "        if (emissiveSupported) {\n"
              "          const float4 accumulatedRadiance = path.accumulatedRadiance + emitted;\n"
              "          next = terminatedPathWithAccumulatedRadiance(path, accumulatedRadiance);\n"
              "          step.emittedRadiance = emitted;\n"
              "          step.continuationThroughput = float4(0.0f);\n"
              "          step.flags = next.flags;\n"
              "        } else {\n"
              "          step.event = gpuDiffusePathStepEventUnsupported;\n"
              "          next.flags = unsupportedPathFlags(path.flags);\n"
              "          step.continuationThroughput = float4(0.0f);\n"
              "          step.flags = next.flags;\n"
              "        }\n"
              "      }\n"
              "    } else {\n"
              "      const float4 contribution = path.throughput *\n"
              "          missRadiance(parameters, sceneUpload, path);\n"
              "      next = terminatedPathWithAccumulatedRadiance(\n"
              "          path, path.accumulatedRadiance + contribution);\n"
              "      step.event = gpuDiffusePathStepEventMiss;\n"
              "      step.missRadiance = contribution;\n"
              "      step.continuationThroughput = float4(0.0f);\n"
              "      step.flags = next.flags;\n"
              "    }\n"
              "  }\n"
              "  return step;\n"
              "}\n"
              "device float4* accumulationColorSums(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device uchar* accumulation) {\n"
              "  (void)parameters;\n"
              "  return reinterpret_cast<device float4*>(accumulation);\n"
              "}\n"
              "device uint* accumulationSampleCounts(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device uchar* accumulation) {\n"
              "  const uint pixelCount = parameters.imageWidth * parameters.imageHeight;\n"
              "  return reinterpret_cast<device uint*>(accumulation + pixelCount * 16u);\n"
              "}\n"
              "uint accumulationIndexFor(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    uint pathIndex,\n"
              "    GpuDiffusePathStateRecord path) {\n"
              "  if (parameters.accumulationTargetMode == gpuDiffusePathLoopAccumulationTargetPath) {\n"
              "    return pathIndex;\n"
              "  }\n"
              "  if (parameters.accumulationTargetMode == gpuDiffusePathLoopAccumulationTargetSampleSlot) {\n"
              "    return path.pixelIndex * parameters.imageHeight + path.primarySampleIndex;\n"
              "  }\n"
              "  return path.pixelIndex;\n"
              "}\n"
              "void accumulateTerminatedPath(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device uchar* accumulation,\n"
              "    uint pathIndex,\n"
              "    GpuDiffusePathStateRecord path) {\n"
              "  const uint pixelCount = parameters.imageWidth * parameters.imageHeight;\n"
              "  const uint accumulationIndex = accumulationIndexFor(parameters, pathIndex, path);\n"
              "  if (accumulationIndex >= pixelCount) {\n"
              "    return;\n"
              "  }\n"
              "  accumulationColorSums(parameters, accumulation)[accumulationIndex] =\n"
              "      path.accumulatedRadiance;\n"
              "  accumulationSampleCounts(parameters, accumulation)[accumulationIndex] = 1u;\n"
              "}\n"
              "void recordRetainedActivePath(device atomic_uint* retainedIndices,\n"
              "                              uint pathIndex,\n"
              "                              GpuDiffusePathStateRecord path) {\n"
              "  if (!pathStateIsActive(path)) {\n"
              "    return;\n"
              "  }\n"
              "  const uint slot = atomic_fetch_add_explicit(&retainedIndices[0], 1u,\n"
              "                                             memory_order_relaxed);\n"
              "  atomic_store_explicit(&retainedIndices[slot + 1u], pathIndex,\n"
              "                        memory_order_relaxed);\n"
              "}\n"
              "kernel void clearDiffusePathLoopAccumulation(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters [[buffer(0)]],\n"
              "    device uchar* accumulation [[buffer(1)]],\n"
              "    uint id [[thread_position_in_grid]]) {\n"
              "  const uint pixelCount = parameters.imageWidth * parameters.imageHeight;\n"
              "  if (id >= pixelCount) {\n"
              "    return;\n"
              "  }\n"
              "  accumulationColorSums(parameters, accumulation)[id] = float4(0.0f);\n"
              "  accumulationSampleCounts(parameters, accumulation)[id] = 0u;\n"
              "}\n"
              "kernel void probeDiffusePathLoopLaunch(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters [[buffer(0)]],\n"
              "    device GpuDiffusePathLoopLaunchParameters* echoedParameters [[buffer(1)]],\n"
              "    device const uchar* sceneUpload [[buffer(2)]],\n"
              "    device const GpuDiffusePathStateRecord* initialPathStates [[buffer(3)]],\n"
              "    device GpuDiffusePathStateRecord* activePathStates [[buffer(4)]],\n"
              "    device GpuDiffusePathStateRecord* nextPathStates [[buffer(5)]],\n"
              "    device GpuDiffusePathStepRecord* stepRecords [[buffer(6)]],\n"
              "    device atomic_uint* retainedIndices [[buffer(7)]],\n"
              "    device uchar* accumulation [[buffer(8)]],\n"
              "    uint id [[thread_position_in_grid]]) {\n"
              "  if (id == 0u) {\n"
              "    echoedParameters[0] = parameters;\n"
              "  }\n"
              "  if (id >= parameters.initialPathCount) {\n"
              "    return;\n"
              "  }\n"
              "  GpuDiffusePathStateRecord path = initialPathFor(parameters, initialPathStates, id);\n"
              "  activePathStates[id] = path;\n"
              "  nextPathStates[id] = activePathStates[id];\n"
              "  stepRecords[id] = inactiveStep(id, path);\n"
              "  recordRetainedActivePath(retainedIndices, id, activePathStates[id]);\n"
              "  (void)sceneUpload;\n"
              "}\n"
              "kernel void probeDiffusePathLoopAllMiss(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters [[buffer(0)]],\n"
              "    device GpuDiffusePathLoopLaunchParameters* echoedParameters [[buffer(1)]],\n"
              "    device const uchar* sceneUpload [[buffer(2)]],\n"
              "    device const GpuDiffusePathStateRecord* initialPathStates [[buffer(3)]],\n"
              "    device GpuDiffusePathStateRecord* activePathStates [[buffer(4)]],\n"
              "    device GpuDiffusePathStateRecord* nextPathStates [[buffer(5)]],\n"
              "    device GpuDiffusePathStepRecord* stepRecords [[buffer(6)]],\n"
              "    device atomic_uint* retainedIndices [[buffer(7)]],\n"
              "    device uchar* accumulation [[buffer(8)]],\n"
              "    uint id [[thread_position_in_grid]]) {\n"
              "  if (id == 0u) {\n"
              "    echoedParameters[0] = parameters;\n"
              "  }\n"
              "  if (id >= parameters.initialPathCount) {\n"
              "    return;\n"
              "  }\n"
              "  GpuDiffusePathStateRecord path = initialPathFor(parameters, initialPathStates, id);\n"
              "  GpuDiffusePathStepRecord step = inactiveStep(id, path);\n"
              "  if (pathStateIsActive(path)) {\n"
              "    float4 contribution = path.throughput * missRadiance(parameters, sceneUpload, path);\n"
              "    path.accumulatedRadiance += contribution;\n"
              "    path.flags = (path.flags & ~gpuDiffusePathStateActiveFlag) |\n"
              "                 gpuDiffusePathStateTerminatedFlag;\n"
              "    accumulateTerminatedPath(parameters, accumulation, id, path);\n"
              "    step.event = gpuDiffusePathStepEventMiss;\n"
              "    step.missRadiance = contribution;\n"
              "    step.continuationThroughput = float4(0.0f);\n"
              "    step.flags = path.flags;\n"
              "  }\n"
              "  activePathStates[id] = path;\n"
              "  nextPathStates[id] = path;\n"
              "  stepRecords[id] = step;\n"
              "  recordRetainedActivePath(retainedIndices, id, path);\n"
              "}\n"
              "kernel void probeDiffusePathLoopClosestHit(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters [[buffer(0)]],\n"
              "    device GpuDiffusePathLoopLaunchParameters* echoedParameters [[buffer(1)]],\n"
              "    device const uchar* sceneUpload [[buffer(2)]],\n"
              "    device const GpuDiffusePathStateRecord* initialPathStates [[buffer(3)]],\n"
              "    device GpuDiffusePathStateRecord* activePathStates [[buffer(4)]],\n"
              "    device GpuDiffusePathStateRecord* nextPathStates [[buffer(5)]],\n"
              "    device GpuDiffusePathStepRecord* stepRecords [[buffer(6)]],\n"
              "    device atomic_uint* retainedIndices [[buffer(7)]],\n"
              "    device uchar* accumulation [[buffer(8)]],\n"
              "    device GpuIntersectionHitRecord* closestHits [[buffer(9)]],\n"
              "    uint id [[thread_position_in_grid]]) {\n"
              "  if (id == 0u) {\n"
              "    echoedParameters[0] = parameters;\n"
              "  }\n"
              "  if (id >= parameters.initialPathCount) {\n"
              "    return;\n"
              "  }\n"
              "  GpuDiffusePathStateRecord path = initialPathFor(parameters, initialPathStates, id);\n"
              "  GpuDiffusePathStepRecord step = inactiveStep(id, path);\n"
              "  GpuIntersectionHitRecord hit = missHitRecord(path.ray);\n"
              "  if (pathStateIsActive(path)) {\n"
              "    hit = closestSupportedHit(parameters, sceneUpload, path.ray);\n"
              "    if (hit.hit != 0u) {\n"
              "      step.event = gpuDiffusePathStepEventHit;\n"
              "      step.material = hit.material;\n"
              "      step.object = hit.object;\n"
              "      step.flags = path.flags;\n"
              "    } else {\n"
              "      step.event = gpuDiffusePathStepEventMiss;\n"
              "      step.flags = path.flags;\n"
              "    }\n"
              "  }\n"
              "  activePathStates[id] = path;\n"
              "  nextPathStates[id] = path;\n"
              "  stepRecords[id] = step;\n"
              "  closestHits[id] = hit;\n"
              "  recordRetainedActivePath(retainedIndices, id, path);\n"
              "  (void)accumulation;\n"
              "}\n"
              "kernel void probeDiffusePathLoopMatteContinuation(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters [[buffer(0)]],\n"
              "    device GpuDiffusePathLoopLaunchParameters* echoedParameters [[buffer(1)]],\n"
              "    device const uchar* sceneUpload [[buffer(2)]],\n"
              "    device const GpuDiffusePathStateRecord* initialPathStates [[buffer(3)]],\n"
              "    device GpuDiffusePathStateRecord* activePathStates [[buffer(4)]],\n"
              "    device GpuDiffusePathStateRecord* nextPathStates [[buffer(5)]],\n"
              "    device GpuDiffusePathStepRecord* stepRecords [[buffer(6)]],\n"
              "    device atomic_uint* retainedIndices [[buffer(7)]],\n"
              "    device uchar* accumulation [[buffer(8)]],\n"
              "    device GpuIntersectionHitRecord* closestHits [[buffer(9)]],\n"
              "    uint id [[thread_position_in_grid]]) {\n"
              "  if (id == 0u) {\n"
              "    echoedParameters[0] = parameters;\n"
              "  }\n"
              "  if (id >= parameters.initialPathCount) {\n"
              "    return;\n"
              "  }\n"
              "  GpuDiffusePathStateRecord path = initialPathFor(parameters, initialPathStates, id);\n"
              "  GpuDiffusePathStateRecord next = path;\n"
              "  GpuIntersectionHitRecord hit = missHitRecord(path.ray);\n"
              "  const GpuDiffusePathStepRecord step = mattePathStep(\n"
              "      parameters, sceneUpload, id, path, next, hit);\n"
              "  if (pathStateIsActive(path) && pathStateIsTerminated(next)) {\n"
              "    accumulateTerminatedPath(parameters, accumulation, id, next);\n"
              "  }\n"
              "  activePathStates[id] = path;\n"
              "  nextPathStates[id] = next;\n"
              "  stepRecords[id] = step;\n"
              "  closestHits[id] = hit;\n"
              "  recordRetainedActivePath(retainedIndices, id, next);\n"
              "}\n"
              "kernel void runDiffusePathLoopMatteSubset(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters [[buffer(0)]],\n"
              "    device GpuDiffusePathLoopLaunchParameters* echoedParameters [[buffer(1)]],\n"
              "    device const uchar* sceneUpload [[buffer(2)]],\n"
              "    device const GpuDiffusePathStateRecord* initialPathStates [[buffer(3)]],\n"
              "    device GpuDiffusePathStateRecord* activePathStates [[buffer(4)]],\n"
              "    device GpuDiffusePathStateRecord* nextPathStates [[buffer(5)]],\n"
              "    device GpuDiffusePathStepRecord* stepRecords [[buffer(6)]],\n"
              "    device atomic_uint* retainedIndices [[buffer(7)]],\n"
              "    device uchar* accumulation [[buffer(8)]],\n"
              "    device GpuIntersectionHitRecord* closestHits [[buffer(9)]],\n"
              "    uint id [[thread_position_in_grid]]) {\n"
              "  if (id == 0u) {\n"
              "    echoedParameters[0] = parameters;\n"
              "  }\n"
              "  if (id >= parameters.initialPathCount) {\n"
              "    return;\n"
              "  }\n"
              "  GpuDiffusePathStateRecord path = initialPathFor(parameters, initialPathStates, id);\n"
              "  if (parameters.captureDiagnostics != 0u) {\n"
              "    activePathStates[id] = path;\n"
              "  }\n"
              "  GpuIntersectionHitRecord lastHit = missHitRecord(path.ray);\n"
              "  if (pathStateIsActive(path) && path.depth >= parameters.maxDepth) {\n"
              "    path.flags = terminatedPathFlags(path.flags);\n"
              "    accumulateTerminatedPath(parameters, accumulation, id, path);\n"
              "  }\n"
              "  for (uint depthIndex = 0u; depthIndex != parameters.maxDepth; ++depthIndex) {\n"
              "    if (!pathStateIsActive(path) || path.depth >= parameters.maxDepth) {\n"
              "      break;\n"
              "    }\n"
              "    GpuDiffusePathStateRecord next = path;\n"
              "    GpuIntersectionHitRecord hit = missHitRecord(path.ray);\n"
              "    GpuDiffusePathStepRecord step = mattePathStep(\n"
              "        parameters, sceneUpload, id, path, next, hit);\n"
              "    if (parameters.captureDiagnostics != 0u) {\n"
              "      stepRecords[id * parameters.maxDepth + depthIndex] = step;\n"
              "    }\n"
              "    lastHit = hit;\n"
              "    if (pathStateIsActive(path) && pathStateIsTerminated(next)) {\n"
              "      accumulateTerminatedPath(parameters, accumulation, id, next);\n"
              "    }\n"
              "    if (pathStateIsActive(next) && next.depth >= parameters.maxDepth) {\n"
              "      next.flags = terminatedPathFlags(next.flags);\n"
              "      accumulateTerminatedPath(parameters, accumulation, id, next);\n"
              "    }\n"
              "    path = next;\n"
              "  }\n"
              "  if (parameters.captureDiagnostics != 0u) {\n"
              "    nextPathStates[id] = path;\n"
              "    closestHits[id] = lastHit;\n"
              "    recordRetainedActivePath(retainedIndices, id, path);\n"
              "  }\n"
              "}\n"
              "kernel void probeDiffusePathLoopMatteHitShading(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters [[buffer(0)]],\n"
              "    device GpuDiffusePathLoopLaunchParameters* echoedParameters [[buffer(1)]],\n"
              "    device const uchar* sceneUpload [[buffer(2)]],\n"
              "    device const GpuDiffusePathStateRecord* initialPathStates [[buffer(3)]],\n"
              "    device GpuDiffusePathStateRecord* activePathStates [[buffer(4)]],\n"
              "    device GpuDiffusePathStateRecord* nextPathStates [[buffer(5)]],\n"
              "    device GpuDiffusePathStepRecord* stepRecords [[buffer(6)]],\n"
              "    device atomic_uint* retainedIndices [[buffer(7)]],\n"
              "    device uchar* accumulation [[buffer(8)]],\n"
              "    device GpuIntersectionHitRecord* closestHits [[buffer(9)]],\n"
              "    uint id [[thread_position_in_grid]]) {\n"
              "  if (id == 0u) {\n"
              "    echoedParameters[0] = parameters;\n"
              "  }\n"
              "  if (id >= parameters.initialPathCount) {\n"
              "    return;\n"
              "  }\n"
              "  GpuDiffusePathStateRecord path = initialPathFor(parameters, initialPathStates, id);\n"
              "  GpuDiffusePathStepRecord step = inactiveStep(id, path);\n"
              "  GpuIntersectionHitRecord hit = missHitRecord(path.ray);\n"
              "  if (pathStateIsActive(path)) {\n"
              "    hit = closestSupportedHit(parameters, sceneUpload, path.ray);\n"
              "    if (hit.hit != 0u) {\n"
              "      bool supported = false;\n"
              "      const float4 continuation = matteContinuationThroughput(\n"
              "          parameters, sceneUpload, hit, path, supported);\n"
              "      step.event = supported ? gpuDiffusePathStepEventHit :\n"
              "                              gpuDiffusePathStepEventUnsupported;\n"
              "      step.material = hit.material;\n"
              "      step.object = hit.object;\n"
              "      step.flags = supported ? path.flags : unsupportedPathFlags(path.flags);\n"
              "      step.continuationThroughput = supported ? continuation : float4(0.0f);\n"
              "    } else {\n"
              "      step.event = gpuDiffusePathStepEventMiss;\n"
              "      step.flags = path.flags;\n"
              "    }\n"
              "  }\n"
              "  activePathStates[id] = path;\n"
              "  nextPathStates[id] = path;\n"
              "  stepRecords[id] = step;\n"
              "  closestHits[id] = hit;\n"
              "  recordRetainedActivePath(retainedIndices, id, path);\n"
              "  (void)accumulation;\n"
              "}\n";
    }

    NSString* diffusePathLoopKernelSource() {
      return [diffusePathLoopKernelSourcePrefix()
        stringByAppendingString:diffusePathLoopKernelSourceSuffix()];
    }

    id<MTLComputePipelineState> newPipeline(id<MTLDevice> device, NSString* functionName) {
      NSError* error = nil;
      id<MTLLibrary> library = [device newLibraryWithSource:diffusePathLoopKernelSource()
                                                    options:nil
                                                      error:&error];
      if (!library) {
        throw metalError("Metal diffuse path-loop shader compilation failed", error);
      }

      id<MTLFunction> function = [library newFunctionWithName:functionName];
      if (!function) {
        throw std::runtime_error("Metal diffuse path-loop probe function was not found");
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
        return device ? newPipeline(device, @"probeDiffusePathLoopLaunch") : nil;
      }();
      return pipeline;
    }

    id<MTLComputePipelineState> sharedAllMissProbePipeline() {
      static id<MTLComputePipelineState> pipeline = [] {
        id<MTLDevice> device = sharedMetalDevice();
        return device ? newPipeline(device, @"probeDiffusePathLoopAllMiss") : nil;
      }();
      return pipeline;
    }

    id<MTLComputePipelineState> sharedClosestHitProbePipeline() {
      static id<MTLComputePipelineState> pipeline = [] {
        id<MTLDevice> device = sharedMetalDevice();
        return device ? newPipeline(device, @"probeDiffusePathLoopClosestHit") : nil;
      }();
      return pipeline;
    }

    id<MTLComputePipelineState> sharedMatteHitShadingProbePipeline() {
      static id<MTLComputePipelineState> pipeline = [] {
        id<MTLDevice> device = sharedMetalDevice();
        return device ? newPipeline(device, @"probeDiffusePathLoopMatteHitShading") : nil;
      }();
      return pipeline;
    }

    id<MTLComputePipelineState> sharedMatteContinuationProbePipeline() {
      static id<MTLComputePipelineState> pipeline = [] {
        id<MTLDevice> device = sharedMetalDevice();
        return device ? newPipeline(device, @"probeDiffusePathLoopMatteContinuation") : nil;
      }();
      return pipeline;
    }

    id<MTLComputePipelineState> sharedMattePathLoopPipeline() {
      static id<MTLComputePipelineState> pipeline = [] {
        id<MTLDevice> device = sharedMetalDevice();
        return device ? newPipeline(device, @"runDiffusePathLoopMatteSubset") : nil;
      }();
      return pipeline;
    }

    id<MTLComputePipelineState> sharedClearAccumulationPipeline() {
      static id<MTLComputePipelineState> pipeline = [] {
        id<MTLDevice> device = sharedMetalDevice();
        return device ? newPipeline(device, @"clearDiffusePathLoopAccumulation") : nil;
      }();
      return pipeline;
    }

    NSUInteger bufferLength(std::uint64_t requestedBytes) {
      return static_cast<NSUInteger>(std::max<std::uint64_t>(1, requestedBytes));
    }

    std::uint64_t pixelCount(const GpuDiffusePathLoopLaunchParameters& parameters) {
      return static_cast<std::uint64_t>(parameters.imageWidth) *
             static_cast<std::uint64_t>(parameters.imageHeight);
    }

    void clearRetainedIndexBuffer(id<MTLBuffer> retainedIndexBuffer, std::uint64_t bytes) {
      if (bytes == 0) {
        return;
      }
      std::memset([retainedIndexBuffer contents], 0, static_cast<std::size_t>(bytes));
    }

    std::vector<std::uint32_t> retainedPathIndicesFromBuffer(
      id<MTLBuffer> retainedIndexBuffer, std::size_t maxPathCount) {
      const auto* retainedIndices =
        static_cast<const std::uint32_t*>([retainedIndexBuffer contents]);
      const std::uint32_t retainedCount = retainedIndices[0];
      if (retainedCount > maxPathCount) {
        throw std::runtime_error(
          "Metal diffuse path-loop retained path count exceeds initial path count");
      }
      return std::vector<std::uint32_t>(retainedIndices + 1, retainedIndices + 1 + retainedCount);
    }

    void validateUniqueActiveAccumulationTargets(
      const GpuDiffusePathLoopLaunchPlan& plan,
      const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
      const char* probeName) {
      const std::uint64_t pixels = pixelCount(plan.parameters);
      if (pixels > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::overflow_error(std::string("Metal diffuse path-loop ") + probeName +
                                  " pixel count overflows");
      }
      if (plan.parameters.accumulationTargetMode == gpuDiffusePathLoopAccumulationTargetPath) {
        if (pixels < initialPathStates.size()) {
          throw std::invalid_argument(
            std::string("Metal diffuse path-loop ") + probeName +
            " path accumulation layout has fewer slots than paths");
        }
        return;
      }
      if (plan.parameters.accumulationTargetMode ==
          gpuDiffusePathLoopAccumulationTargetSampleSlot) {
        std::vector<bool> seenSlots(static_cast<std::size_t>(pixels), false);
        for (const GpuDiffusePathStateRecord& path : initialPathStates) {
          if (!gpuDiffusePathStateIsActive(path)) {
            continue;
          }
          if (path.pixelIndex >= plan.parameters.imageWidth) {
            throw std::invalid_argument(
              std::string("Metal diffuse path-loop ") + probeName +
              " path pixel is outside the sample-slot accumulation layout");
          }
          if (path.primarySampleIndex >= plan.parameters.imageHeight) {
            throw std::invalid_argument(
              std::string("Metal diffuse path-loop ") + probeName +
              " path sample is outside the sample-slot accumulation layout");
          }
          const std::uint64_t slot =
            static_cast<std::uint64_t>(path.pixelIndex) *
              static_cast<std::uint64_t>(plan.parameters.imageHeight) +
            static_cast<std::uint64_t>(path.primarySampleIndex);
          if (slot >= pixels) {
            throw std::invalid_argument(
              std::string("Metal diffuse path-loop ") + probeName +
              " path sample slot is outside the accumulation layout");
          }
          if (seenSlots[static_cast<std::size_t>(slot)]) {
            throw std::invalid_argument(
              std::string("Metal diffuse path-loop ") + probeName +
              " probe requires unique active pixel/sample targets");
          }
          seenSlots[static_cast<std::size_t>(slot)] = true;
        }
        return;
      }
      std::vector<bool> seenPixels(static_cast<std::size_t>(pixels), false);
      for (const GpuDiffusePathStateRecord& path : initialPathStates) {
        if (!gpuDiffusePathStateIsActive(path)) {
          continue;
        }
        if (path.pixelIndex >= pixels) {
          throw std::invalid_argument(
            std::string("Metal diffuse path-loop ") + probeName +
            " path pixel is outside the accumulation layout");
        }
        if (seenPixels[path.pixelIndex]) {
          throw std::invalid_argument(
            std::string("Metal diffuse path-loop ") + probeName +
            " probe requires unique active pixel targets");
        }
        seenPixels[path.pixelIndex] = true;
      }
    }

    void validateClosestHitProbeGeometry(const GpuDiffusePathLoopLaunchPlan& plan) {
      const GpuDiffusePathLoopLaunchParameters& parameters = plan.parameters;
      if (parameters.primitiveCount == 0u || parameters.bvhNodeCount == 0u) {
        throw std::invalid_argument(
          "Metal diffuse path-loop closest-hit probe requires compiled geometry");
      }
      const std::uint32_t supportedPrimitiveCount =
        parameters.triangleCount + parameters.sphereCount + parameters.planeCount +
        parameters.rectangleCount + parameters.diskCount + parameters.openCylinderCount +
        parameters.torusCount;
      if (supportedPrimitiveCount != parameters.primitiveCount) {
        throw std::invalid_argument(
          "Metal diffuse path-loop closest-hit probe requires one supported payload per primitive");
      }
    }

    void validateMatteHitShadingProbeScene(const GpuDiffusePathLoopLaunchPlan& plan) {
      validateClosestHitProbeGeometry(plan);
      if (plan.parameters.materialCount == 0u || plan.parameters.textureCount == 0u) {
        throw std::invalid_argument(
          "Metal diffuse path-loop matte shading probe requires material and texture records");
      }
    }

    void validateMattePathLoopScene(const GpuDiffusePathLoopLaunchPlan& plan) {
      if (plan.parameters.primitiveCount == 0u && plan.parameters.bvhNodeCount == 0u) {
        return;
      }
      validateMatteHitShadingProbeScene(plan);
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
  MetalGpuDiffusePathLoopKernel::runLaunchProbe(
    const GpuDiffusePathLoopLaunchPlan& plan,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates) const {
    if (plan.parameters.layoutVersion != gpuDiffusePathLoopLaunchLayoutVersion) {
      throw std::invalid_argument("Metal diffuse path-loop launch descriptor version mismatch");
    }
    if (plan.parameters.maxDepth == 0) {
      throw std::invalid_argument("Metal diffuse path-loop launch requires positive max depth");
    }
    if (initialPathStates.size() != plan.parameters.initialPathCount) {
      throw std::invalid_argument(
        "Metal diffuse path-loop initial path-state count does not match launch descriptor");
    }
    if (plan.sceneUpload.size() != plan.buffers.sceneUploadBytes) {
      throw std::invalid_argument(
        "Metal diffuse path-loop scene upload bytes do not match launch descriptor");
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
        plan.sceneUpload.empty()
          ? [device newBufferWithLength:1 options:MTLResourceStorageModeShared]
          : [device newBufferWithBytes:plan.sceneUpload.data()
                                length:plan.buffers.sceneUploadBytes
                               options:MTLResourceStorageModeShared];
      id<MTLBuffer> initialPathBuffer =
        plan.generatesPrimaryPathsOnDevice() || initialPathStates.empty()
          ? [device newBufferWithLength:1 options:MTLResourceStorageModeShared]
          : [device newBufferWithBytes:initialPathStates.data()
                                length:plan.buffers.initialPathStateBytes
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
      clearRetainedIndexBuffer(retainedIndexBuffer, plan.buffers.retainedIndexBytes);
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
      dispatch1D(encoder, pipeline, static_cast<NSUInteger>(initialPathStates.size()));
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
      result.copiedInitialPathStates.resize(initialPathStates.size());
      if (!result.copiedInitialPathStates.empty()) {
        std::memcpy(result.copiedInitialPathStates.data(), [activePathBuffer contents],
                    result.copiedInitialPathStates.size() * sizeof(GpuDiffusePathStateRecord));
      }
      result.stepRecords.resize(initialPathStates.size());
      if (!result.stepRecords.empty()) {
        std::memcpy(result.stepRecords.data(), [stepRecordBuffer contents],
                    result.stepRecords.size() * sizeof(GpuDiffusePathStepRecord));
      }
      result.retainedPathIndices =
        retainedPathIndicesFromBuffer(retainedIndexBuffer, initialPathStates.size());
      result.readbackWorkerSeconds =
        elapsedSeconds(readbackStart, std::chrono::steady_clock::now());
      return result;
    }
  }

  MetalGpuDiffusePathLoopKernelResult
  MetalGpuDiffusePathLoopKernel::runAllMissProbe(
    const GpuDiffusePathLoopLaunchPlan& plan,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates) const {
    if (plan.parameters.layoutVersion != gpuDiffusePathLoopLaunchLayoutVersion) {
      throw std::invalid_argument("Metal diffuse path-loop launch descriptor version mismatch");
    }
    if (plan.parameters.maxDepth == 0) {
      throw std::invalid_argument("Metal diffuse path-loop all-miss probe requires positive max depth");
    }
    if (plan.parameters.primitiveCount != 0u || plan.parameters.bvhNodeCount != 0u) {
      throw std::invalid_argument(
        "Metal diffuse path-loop all-miss probe requires an empty compiled geometry section");
    }
    if (initialPathStates.size() != plan.parameters.initialPathCount) {
      throw std::invalid_argument(
        "Metal diffuse path-loop initial path-state count does not match launch descriptor");
    }
    if (plan.sceneUpload.size() != plan.buffers.sceneUploadBytes) {
      throw std::invalid_argument(
        "Metal diffuse path-loop scene upload bytes do not match launch descriptor");
    }
    validateUniqueActiveAccumulationTargets(plan, initialPathStates, "all-miss");

    @autoreleasepool {
      if (!launchPathAvailable()) {
        throw std::runtime_error(launchPathUnavailableReason());
      }

      id<MTLDevice> device = sharedMetalDevice();
      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLComputePipelineState> pipeline = sharedAllMissProbePipeline();
      id<MTLComputePipelineState> clearPipeline = sharedClearAccumulationPipeline();
      if (!pipeline) {
        throw std::runtime_error("Metal diffuse path-loop all-miss probe pipeline was not created");
      }
      if (!clearPipeline) {
        throw std::runtime_error(
          "Metal diffuse path-loop accumulation clear pipeline was not created");
      }

      const auto uploadStart = std::chrono::steady_clock::now();
      id<MTLBuffer> parameterBuffer =
        [device newBufferWithBytes:&plan.parameters
                            length:sizeof(GpuDiffusePathLoopLaunchParameters)
                           options:MTLResourceStorageModeShared];
      id<MTLBuffer> echoedParameterBuffer =
        [device newBufferWithLength:sizeof(GpuDiffusePathLoopLaunchParameters)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> sceneUploadBuffer =
        plan.sceneUpload.empty()
          ? [device newBufferWithLength:1 options:MTLResourceStorageModeShared]
          : [device newBufferWithBytes:plan.sceneUpload.data()
                                length:plan.buffers.sceneUploadBytes
                               options:MTLResourceStorageModeShared];
      id<MTLBuffer> initialPathBuffer =
        plan.generatesPrimaryPathsOnDevice() || initialPathStates.empty()
          ? [device newBufferWithLength:1 options:MTLResourceStorageModeShared]
          : [device newBufferWithBytes:initialPathStates.data()
                                length:plan.buffers.initialPathStateBytes
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
        throw std::runtime_error("Metal diffuse path-loop all-miss probe buffer allocation failed");
      }
      clearRetainedIndexBuffer(retainedIndexBuffer, plan.buffers.retainedIndexBytes);
      MetalGpuDiffusePathLoopKernelResult result;
      result.executionPath = "metal_diffuse_path_loop_all_miss_probe";
      result.bufferSizes = plan.buffers;
      result.uploadWorkerSeconds = elapsedSeconds(uploadStart, std::chrono::steady_clock::now());

      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!commandBuffer || !encoder) {
        throw std::runtime_error("Metal diffuse path-loop all-miss probe command setup failed");
      }

      const NSUInteger pixels = static_cast<NSUInteger>(pixelCount(plan.parameters));
      [encoder setComputePipelineState:clearPipeline];
      [encoder setBuffer:parameterBuffer offset:0 atIndex:0];
      [encoder setBuffer:accumulationBuffer offset:0 atIndex:1];
      dispatch1D(encoder, clearPipeline, pixels);

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
      dispatch1D(encoder, pipeline, static_cast<NSUInteger>(initialPathStates.size()));
      [encoder endEncoding];

      const auto kernelStart = std::chrono::steady_clock::now();
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      result.kernelWorkerSeconds = elapsedSeconds(kernelStart, std::chrono::steady_clock::now());
      if (commandBuffer.status == MTLCommandBufferStatusError) {
        throw metalError("Metal diffuse path-loop all-miss probe dispatch failed",
                         commandBuffer.error);
      }

      const auto readbackStart = std::chrono::steady_clock::now();
      std::memcpy(&result.echoedParameters, [echoedParameterBuffer contents],
                  sizeof(result.echoedParameters));
      result.resolvedPathStates.resize(initialPathStates.size());
      if (!result.resolvedPathStates.empty()) {
        std::memcpy(result.resolvedPathStates.data(), [activePathBuffer contents],
                    result.resolvedPathStates.size() * sizeof(GpuDiffusePathStateRecord));
      }
      result.stepRecords.resize(initialPathStates.size());
      if (!result.stepRecords.empty()) {
        std::memcpy(result.stepRecords.data(), [stepRecordBuffer contents],
                    result.stepRecords.size() * sizeof(GpuDiffusePathStepRecord));
      }
      result.retainedPathIndices =
        retainedPathIndicesFromBuffer(retainedIndexBuffer, initialPathStates.size());
      result.accumulationColorSums.resize(pixelCount(plan.parameters));
      if (!result.accumulationColorSums.empty()) {
        std::memcpy(result.accumulationColorSums.data(), [accumulationBuffer contents],
                    result.accumulationColorSums.size() * sizeof(std::array<float, 4>));
      }
      result.accumulationSampleCounts.resize(pixelCount(plan.parameters));
      if (!result.accumulationSampleCounts.empty()) {
        const std::uint64_t colorBytes =
          result.accumulationColorSums.size() * sizeof(std::array<float, 4>);
        const auto* sampleCountBytes =
          static_cast<const std::uint8_t*>([accumulationBuffer contents]) + colorBytes;
        std::memcpy(result.accumulationSampleCounts.data(), sampleCountBytes,
                    result.accumulationSampleCounts.size() * sizeof(std::uint32_t));
      }
      result.readbackWorkerSeconds =
        elapsedSeconds(readbackStart, std::chrono::steady_clock::now());
      return result;
    }
  }

  MetalGpuDiffusePathLoopKernelResult
  MetalGpuDiffusePathLoopKernel::runClosestHitProbe(
    const GpuDiffusePathLoopLaunchPlan& plan,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates) const {
    if (plan.parameters.layoutVersion != gpuDiffusePathLoopLaunchLayoutVersion) {
      throw std::invalid_argument("Metal diffuse path-loop launch descriptor version mismatch");
    }
    if (plan.parameters.maxDepth == 0) {
      throw std::invalid_argument(
        "Metal diffuse path-loop closest-hit probe requires positive max depth");
    }
    if (initialPathStates.size() != plan.parameters.initialPathCount) {
      throw std::invalid_argument(
        "Metal diffuse path-loop initial path-state count does not match launch descriptor");
    }
    if (plan.sceneUpload.size() != plan.buffers.sceneUploadBytes) {
      throw std::invalid_argument(
        "Metal diffuse path-loop scene upload bytes do not match launch descriptor");
    }
    validateClosestHitProbeGeometry(plan);

    @autoreleasepool {
      if (!launchPathAvailable()) {
        throw std::runtime_error(launchPathUnavailableReason());
      }

      id<MTLDevice> device = sharedMetalDevice();
      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLComputePipelineState> pipeline = sharedClosestHitProbePipeline();
      if (!pipeline) {
        throw std::runtime_error(
          "Metal diffuse path-loop closest-hit probe pipeline was not created");
      }

      const auto uploadStart = std::chrono::steady_clock::now();
      id<MTLBuffer> parameterBuffer =
        [device newBufferWithBytes:&plan.parameters
                            length:sizeof(GpuDiffusePathLoopLaunchParameters)
                           options:MTLResourceStorageModeShared];
      id<MTLBuffer> echoedParameterBuffer =
        [device newBufferWithLength:sizeof(GpuDiffusePathLoopLaunchParameters)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> sceneUploadBuffer =
        plan.sceneUpload.empty()
          ? [device newBufferWithLength:1 options:MTLResourceStorageModeShared]
          : [device newBufferWithBytes:plan.sceneUpload.data()
                                length:plan.buffers.sceneUploadBytes
                               options:MTLResourceStorageModeShared];
      id<MTLBuffer> initialPathBuffer =
        plan.generatesPrimaryPathsOnDevice() || initialPathStates.empty()
          ? [device newBufferWithLength:1 options:MTLResourceStorageModeShared]
          : [device newBufferWithBytes:initialPathStates.data()
                                length:plan.buffers.initialPathStateBytes
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
      id<MTLBuffer> closestHitBuffer =
        [device newBufferWithLength:bufferLength(initialPathStates.size() *
                                                sizeof(GpuIntersectionHitRecord))
                            options:MTLResourceStorageModeShared];
      if (!parameterBuffer || !echoedParameterBuffer || !sceneUploadBuffer || !initialPathBuffer ||
          !activePathBuffer || !nextPathBuffer || !stepRecordBuffer || !retainedIndexBuffer ||
          !accumulationBuffer || !closestHitBuffer) {
        throw std::runtime_error(
          "Metal diffuse path-loop closest-hit probe buffer allocation failed");
      }
      clearRetainedIndexBuffer(retainedIndexBuffer, plan.buffers.retainedIndexBytes);
      MetalGpuDiffusePathLoopKernelResult result;
      result.executionPath = "metal_diffuse_path_loop_closest_hit_probe";
      result.bufferSizes = plan.buffers;
      result.uploadWorkerSeconds = elapsedSeconds(uploadStart, std::chrono::steady_clock::now());

      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!commandBuffer || !encoder) {
        throw std::runtime_error("Metal diffuse path-loop closest-hit probe command setup failed");
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
      [encoder setBuffer:closestHitBuffer offset:0 atIndex:9];
      dispatch1D(encoder, pipeline, static_cast<NSUInteger>(initialPathStates.size()));
      [encoder endEncoding];

      const auto kernelStart = std::chrono::steady_clock::now();
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      result.kernelWorkerSeconds = elapsedSeconds(kernelStart, std::chrono::steady_clock::now());
      if (commandBuffer.status == MTLCommandBufferStatusError) {
        throw metalError("Metal diffuse path-loop closest-hit probe dispatch failed",
                         commandBuffer.error);
      }

      const auto readbackStart = std::chrono::steady_clock::now();
      std::memcpy(&result.echoedParameters, [echoedParameterBuffer contents],
                  sizeof(result.echoedParameters));
      result.copiedInitialPathStates.resize(initialPathStates.size());
      if (!result.copiedInitialPathStates.empty()) {
        std::memcpy(result.copiedInitialPathStates.data(), [activePathBuffer contents],
                    result.copiedInitialPathStates.size() * sizeof(GpuDiffusePathStateRecord));
      }
      result.closestHitRecords.resize(initialPathStates.size());
      if (!result.closestHitRecords.empty()) {
        std::memcpy(result.closestHitRecords.data(), [closestHitBuffer contents],
                    result.closestHitRecords.size() * sizeof(GpuIntersectionHitRecord));
      }
      result.stepRecords.resize(initialPathStates.size());
      if (!result.stepRecords.empty()) {
        std::memcpy(result.stepRecords.data(), [stepRecordBuffer contents],
                    result.stepRecords.size() * sizeof(GpuDiffusePathStepRecord));
      }
      result.retainedPathIndices =
        retainedPathIndicesFromBuffer(retainedIndexBuffer, initialPathStates.size());
      result.readbackWorkerSeconds =
        elapsedSeconds(readbackStart, std::chrono::steady_clock::now());
      return result;
    }
  }

  MetalGpuDiffusePathLoopKernelResult
  MetalGpuDiffusePathLoopKernel::runMatteHitShadingProbe(
    const GpuDiffusePathLoopLaunchPlan& plan,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates) const {
    if (plan.parameters.layoutVersion != gpuDiffusePathLoopLaunchLayoutVersion) {
      throw std::invalid_argument("Metal diffuse path-loop launch descriptor version mismatch");
    }
    if (plan.parameters.maxDepth == 0) {
      throw std::invalid_argument(
        "Metal diffuse path-loop matte shading probe requires positive max depth");
    }
    if (initialPathStates.size() != plan.parameters.initialPathCount) {
      throw std::invalid_argument(
        "Metal diffuse path-loop initial path-state count does not match launch descriptor");
    }
    if (plan.sceneUpload.size() != plan.buffers.sceneUploadBytes) {
      throw std::invalid_argument(
        "Metal diffuse path-loop scene upload bytes do not match launch descriptor");
    }
    validateMatteHitShadingProbeScene(plan);

    @autoreleasepool {
      if (!launchPathAvailable()) {
        throw std::runtime_error(launchPathUnavailableReason());
      }

      id<MTLDevice> device = sharedMetalDevice();
      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLComputePipelineState> pipeline = sharedMatteHitShadingProbePipeline();
      if (!pipeline) {
        throw std::runtime_error(
          "Metal diffuse path-loop matte shading probe pipeline was not created");
      }

      const auto uploadStart = std::chrono::steady_clock::now();
      id<MTLBuffer> parameterBuffer =
        [device newBufferWithBytes:&plan.parameters
                            length:sizeof(GpuDiffusePathLoopLaunchParameters)
                           options:MTLResourceStorageModeShared];
      id<MTLBuffer> echoedParameterBuffer =
        [device newBufferWithLength:sizeof(GpuDiffusePathLoopLaunchParameters)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> sceneUploadBuffer =
        plan.sceneUpload.empty()
          ? [device newBufferWithLength:1 options:MTLResourceStorageModeShared]
          : [device newBufferWithBytes:plan.sceneUpload.data()
                                length:plan.buffers.sceneUploadBytes
                               options:MTLResourceStorageModeShared];
      id<MTLBuffer> initialPathBuffer =
        plan.generatesPrimaryPathsOnDevice() || initialPathStates.empty()
          ? [device newBufferWithLength:1 options:MTLResourceStorageModeShared]
          : [device newBufferWithBytes:initialPathStates.data()
                                length:plan.buffers.initialPathStateBytes
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
      id<MTLBuffer> closestHitBuffer =
        [device newBufferWithLength:bufferLength(initialPathStates.size() *
                                                sizeof(GpuIntersectionHitRecord))
                            options:MTLResourceStorageModeShared];
      if (!parameterBuffer || !echoedParameterBuffer || !sceneUploadBuffer || !initialPathBuffer ||
          !activePathBuffer || !nextPathBuffer || !stepRecordBuffer || !retainedIndexBuffer ||
          !accumulationBuffer || !closestHitBuffer) {
        throw std::runtime_error(
          "Metal diffuse path-loop matte shading probe buffer allocation failed");
      }
      clearRetainedIndexBuffer(retainedIndexBuffer, plan.buffers.retainedIndexBytes);
      MetalGpuDiffusePathLoopKernelResult result;
      result.executionPath = "metal_diffuse_path_loop_matte_hit_shading_probe";
      result.bufferSizes = plan.buffers;
      result.uploadWorkerSeconds = elapsedSeconds(uploadStart, std::chrono::steady_clock::now());

      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!commandBuffer || !encoder) {
        throw std::runtime_error(
          "Metal diffuse path-loop matte shading probe command setup failed");
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
      [encoder setBuffer:closestHitBuffer offset:0 atIndex:9];
      dispatch1D(encoder, pipeline, static_cast<NSUInteger>(initialPathStates.size()));
      [encoder endEncoding];

      const auto kernelStart = std::chrono::steady_clock::now();
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      result.kernelWorkerSeconds = elapsedSeconds(kernelStart, std::chrono::steady_clock::now());
      if (commandBuffer.status == MTLCommandBufferStatusError) {
        throw metalError("Metal diffuse path-loop matte shading probe dispatch failed",
                         commandBuffer.error);
      }

      const auto readbackStart = std::chrono::steady_clock::now();
      std::memcpy(&result.echoedParameters, [echoedParameterBuffer contents],
                  sizeof(result.echoedParameters));
      result.copiedInitialPathStates.resize(initialPathStates.size());
      if (!result.copiedInitialPathStates.empty()) {
        std::memcpy(result.copiedInitialPathStates.data(), [activePathBuffer contents],
                    result.copiedInitialPathStates.size() * sizeof(GpuDiffusePathStateRecord));
      }
      result.closestHitRecords.resize(initialPathStates.size());
      if (!result.closestHitRecords.empty()) {
        std::memcpy(result.closestHitRecords.data(), [closestHitBuffer contents],
                    result.closestHitRecords.size() * sizeof(GpuIntersectionHitRecord));
      }
      result.stepRecords.resize(initialPathStates.size());
      if (!result.stepRecords.empty()) {
        std::memcpy(result.stepRecords.data(), [stepRecordBuffer contents],
                    result.stepRecords.size() * sizeof(GpuDiffusePathStepRecord));
      }
      result.retainedPathIndices =
        retainedPathIndicesFromBuffer(retainedIndexBuffer, initialPathStates.size());
      result.readbackWorkerSeconds =
        elapsedSeconds(readbackStart, std::chrono::steady_clock::now());
      return result;
    }
  }

  MetalGpuDiffusePathLoopKernelResult
  MetalGpuDiffusePathLoopKernel::runMatteContinuationProbe(
    const GpuDiffusePathLoopLaunchPlan& plan,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates) const {
    if (plan.parameters.layoutVersion != gpuDiffusePathLoopLaunchLayoutVersion) {
      throw std::invalid_argument("Metal diffuse path-loop launch descriptor version mismatch");
    }
    if (plan.parameters.maxDepth == 0) {
      throw std::invalid_argument(
        "Metal diffuse path-loop matte continuation probe requires positive max depth");
    }
    if (initialPathStates.size() != plan.parameters.initialPathCount) {
      throw std::invalid_argument(
        "Metal diffuse path-loop initial path-state count does not match launch descriptor");
    }
    if (plan.sceneUpload.size() != plan.buffers.sceneUploadBytes) {
      throw std::invalid_argument(
        "Metal diffuse path-loop scene upload bytes do not match launch descriptor");
    }
    validateMatteHitShadingProbeScene(plan);
    validateUniqueActiveAccumulationTargets(plan, initialPathStates, "matte continuation");

    @autoreleasepool {
      if (!launchPathAvailable()) {
        throw std::runtime_error(launchPathUnavailableReason());
      }

      id<MTLDevice> device = sharedMetalDevice();
      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLComputePipelineState> pipeline = sharedMatteContinuationProbePipeline();
      id<MTLComputePipelineState> clearPipeline = sharedClearAccumulationPipeline();
      if (!pipeline) {
        throw std::runtime_error(
          "Metal diffuse path-loop matte continuation probe pipeline was not created");
      }
      if (!clearPipeline) {
        throw std::runtime_error(
          "Metal diffuse path-loop accumulation clear pipeline was not created");
      }

      const auto uploadStart = std::chrono::steady_clock::now();
      id<MTLBuffer> parameterBuffer =
        [device newBufferWithBytes:&plan.parameters
                            length:sizeof(GpuDiffusePathLoopLaunchParameters)
                           options:MTLResourceStorageModeShared];
      id<MTLBuffer> echoedParameterBuffer =
        [device newBufferWithLength:sizeof(GpuDiffusePathLoopLaunchParameters)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> sceneUploadBuffer =
        plan.sceneUpload.empty()
          ? [device newBufferWithLength:1 options:MTLResourceStorageModeShared]
          : [device newBufferWithBytes:plan.sceneUpload.data()
                                length:plan.buffers.sceneUploadBytes
                               options:MTLResourceStorageModeShared];
      id<MTLBuffer> initialPathBuffer =
        plan.generatesPrimaryPathsOnDevice() || initialPathStates.empty()
          ? [device newBufferWithLength:1 options:MTLResourceStorageModeShared]
          : [device newBufferWithBytes:initialPathStates.data()
                                length:plan.buffers.initialPathStateBytes
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
      id<MTLBuffer> closestHitBuffer =
        [device newBufferWithLength:bufferLength(initialPathStates.size() *
                                                sizeof(GpuIntersectionHitRecord))
                            options:MTLResourceStorageModeShared];
      if (!parameterBuffer || !echoedParameterBuffer || !sceneUploadBuffer || !initialPathBuffer ||
          !activePathBuffer || !nextPathBuffer || !stepRecordBuffer || !retainedIndexBuffer ||
          !accumulationBuffer || !closestHitBuffer) {
        throw std::runtime_error(
          "Metal diffuse path-loop matte continuation probe buffer allocation failed");
      }
      clearRetainedIndexBuffer(retainedIndexBuffer, plan.buffers.retainedIndexBytes);
      MetalGpuDiffusePathLoopKernelResult result;
      result.executionPath = "metal_diffuse_path_loop_matte_continuation_probe";
      result.bufferSizes = plan.buffers;
      result.uploadWorkerSeconds = elapsedSeconds(uploadStart, std::chrono::steady_clock::now());

      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!commandBuffer || !encoder) {
        throw std::runtime_error(
          "Metal diffuse path-loop matte continuation probe command setup failed");
      }

      const NSUInteger pixels = static_cast<NSUInteger>(pixelCount(plan.parameters));
      [encoder setComputePipelineState:clearPipeline];
      [encoder setBuffer:parameterBuffer offset:0 atIndex:0];
      [encoder setBuffer:accumulationBuffer offset:0 atIndex:1];
      dispatch1D(encoder, clearPipeline, pixels);

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
      [encoder setBuffer:closestHitBuffer offset:0 atIndex:9];
      dispatch1D(encoder, pipeline, static_cast<NSUInteger>(initialPathStates.size()));
      [encoder endEncoding];

      const auto kernelStart = std::chrono::steady_clock::now();
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      result.kernelWorkerSeconds = elapsedSeconds(kernelStart, std::chrono::steady_clock::now());
      if (commandBuffer.status == MTLCommandBufferStatusError) {
        throw metalError("Metal diffuse path-loop matte continuation probe dispatch failed",
                         commandBuffer.error);
      }

      const auto readbackStart = std::chrono::steady_clock::now();
      std::memcpy(&result.echoedParameters, [echoedParameterBuffer contents],
                  sizeof(result.echoedParameters));
      result.copiedInitialPathStates.resize(initialPathStates.size());
      if (!result.copiedInitialPathStates.empty()) {
        std::memcpy(result.copiedInitialPathStates.data(), [activePathBuffer contents],
                    result.copiedInitialPathStates.size() * sizeof(GpuDiffusePathStateRecord));
      }
      result.nextPathStates.resize(initialPathStates.size());
      if (!result.nextPathStates.empty()) {
        std::memcpy(result.nextPathStates.data(), [nextPathBuffer contents],
                    result.nextPathStates.size() * sizeof(GpuDiffusePathStateRecord));
      }
      result.closestHitRecords.resize(initialPathStates.size());
      if (!result.closestHitRecords.empty()) {
        std::memcpy(result.closestHitRecords.data(), [closestHitBuffer contents],
                    result.closestHitRecords.size() * sizeof(GpuIntersectionHitRecord));
      }
      result.stepRecords.resize(initialPathStates.size());
      if (!result.stepRecords.empty()) {
        std::memcpy(result.stepRecords.data(), [stepRecordBuffer contents],
                    result.stepRecords.size() * sizeof(GpuDiffusePathStepRecord));
      }
      result.retainedPathIndices =
        retainedPathIndicesFromBuffer(retainedIndexBuffer, initialPathStates.size());
      result.accumulationColorSums.resize(pixelCount(plan.parameters));
      if (!result.accumulationColorSums.empty()) {
        std::memcpy(result.accumulationColorSums.data(), [accumulationBuffer contents],
                    result.accumulationColorSums.size() * sizeof(std::array<float, 4>));
      }
      result.accumulationSampleCounts.resize(pixelCount(plan.parameters));
      if (!result.accumulationSampleCounts.empty()) {
        const std::uint64_t colorBytes =
          result.accumulationColorSums.size() * sizeof(std::array<float, 4>);
        const auto* sampleCountBytes =
          static_cast<const std::uint8_t*>([accumulationBuffer contents]) + colorBytes;
        std::memcpy(result.accumulationSampleCounts.data(), sampleCountBytes,
                    result.accumulationSampleCounts.size() * sizeof(std::uint32_t));
      }
      result.readbackWorkerSeconds =
        elapsedSeconds(readbackStart, std::chrono::steady_clock::now());
      return result;
    }
  }

  MetalGpuDiffusePathLoopKernelResult MetalGpuDiffusePathLoopKernel::runMattePathLoop(
    const GpuDiffusePathLoopLaunchPlan& plan,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates) const {
    if (plan.parameters.layoutVersion != gpuDiffusePathLoopLaunchLayoutVersion) {
      throw std::invalid_argument("Metal diffuse path-loop launch descriptor version mismatch");
    }
    if (plan.parameters.maxDepth == 0) {
      throw std::invalid_argument("Metal diffuse path-loop requires positive max depth");
    }
    if (initialPathStates.size() != plan.parameters.initialPathCount) {
      throw std::invalid_argument(
        "Metal diffuse path-loop initial path-state count does not match launch descriptor");
    }
    if (plan.sceneUpload.size() != plan.buffers.sceneUploadBytes) {
      throw std::invalid_argument(
        "Metal diffuse path-loop scene upload bytes do not match launch descriptor");
    }
    validateMattePathLoopScene(plan);
    validateUniqueActiveAccumulationTargets(plan, initialPathStates, "matte path-loop");

    @autoreleasepool {
      if (!launchPathAvailable()) {
        throw std::runtime_error(launchPathUnavailableReason());
      }

      id<MTLDevice> device = sharedMetalDevice();
      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLComputePipelineState> pipeline = sharedMattePathLoopPipeline();
      id<MTLComputePipelineState> clearPipeline = sharedClearAccumulationPipeline();
      if (!pipeline) {
        throw std::runtime_error("Metal diffuse path-loop pipeline was not created");
      }
      if (!clearPipeline) {
        throw std::runtime_error(
          "Metal diffuse path-loop accumulation clear pipeline was not created");
      }

      const auto uploadStart = std::chrono::steady_clock::now();
      id<MTLBuffer> parameterBuffer =
        [device newBufferWithBytes:&plan.parameters
                            length:sizeof(GpuDiffusePathLoopLaunchParameters)
                           options:MTLResourceStorageModeShared];
      id<MTLBuffer> echoedParameterBuffer =
        [device newBufferWithLength:sizeof(GpuDiffusePathLoopLaunchParameters)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> sceneUploadBuffer =
        plan.sceneUpload.empty()
          ? [device newBufferWithLength:1 options:MTLResourceStorageModeShared]
          : [device newBufferWithBytes:plan.sceneUpload.data()
                                length:plan.buffers.sceneUploadBytes
                               options:MTLResourceStorageModeShared];
      id<MTLBuffer> initialPathBuffer =
        plan.generatesPrimaryPathsOnDevice() || initialPathStates.empty()
          ? [device newBufferWithLength:1 options:MTLResourceStorageModeShared]
          : [device newBufferWithBytes:initialPathStates.data()
                                length:plan.buffers.initialPathStateBytes
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
      id<MTLBuffer> closestHitBuffer =
        [device newBufferWithLength:bufferLength(initialPathStates.size() *
                                                sizeof(GpuIntersectionHitRecord))
                            options:MTLResourceStorageModeShared];
      if (!parameterBuffer || !echoedParameterBuffer || !sceneUploadBuffer || !initialPathBuffer ||
          !activePathBuffer || !nextPathBuffer || !stepRecordBuffer || !retainedIndexBuffer ||
          !accumulationBuffer || !closestHitBuffer) {
        throw std::runtime_error("Metal diffuse path-loop buffer allocation failed");
      }
      clearRetainedIndexBuffer(retainedIndexBuffer, plan.buffers.retainedIndexBytes);
      if (plan.buffers.stepRecordBytes != 0u) {
        std::memset([stepRecordBuffer contents], 0,
                    static_cast<std::size_t>(plan.buffers.stepRecordBytes));
      }
      MetalGpuDiffusePathLoopKernelResult result;
      result.executionPath = "metal_diffuse_path_loop";
      result.bufferSizes = plan.buffers;
      result.uploadWorkerSeconds = elapsedSeconds(uploadStart, std::chrono::steady_clock::now());

      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!commandBuffer || !encoder) {
        throw std::runtime_error("Metal diffuse path-loop command setup failed");
      }

      const NSUInteger pixels = static_cast<NSUInteger>(pixelCount(plan.parameters));
      [encoder setComputePipelineState:clearPipeline];
      [encoder setBuffer:parameterBuffer offset:0 atIndex:0];
      [encoder setBuffer:accumulationBuffer offset:0 atIndex:1];
      dispatch1D(encoder, clearPipeline, pixels);

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
      [encoder setBuffer:closestHitBuffer offset:0 atIndex:9];
      dispatch1D(encoder, pipeline, static_cast<NSUInteger>(initialPathStates.size()));
      [encoder endEncoding];

      const auto kernelStart = std::chrono::steady_clock::now();
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      result.kernelWorkerSeconds = elapsedSeconds(kernelStart, std::chrono::steady_clock::now());
      if (commandBuffer.status == MTLCommandBufferStatusError) {
        throw metalError("Metal diffuse path-loop dispatch failed", commandBuffer.error);
      }

      const auto readbackStart = std::chrono::steady_clock::now();
      std::memcpy(&result.echoedParameters, [echoedParameterBuffer contents],
                  sizeof(result.echoedParameters));
      if (plan.parameters.captureDiagnostics != 0u) {
        result.copiedInitialPathStates.resize(initialPathStates.size());
        if (!result.copiedInitialPathStates.empty()) {
          std::memcpy(result.copiedInitialPathStates.data(), [activePathBuffer contents],
                      result.copiedInitialPathStates.size() * sizeof(GpuDiffusePathStateRecord));
        }
        result.nextPathStates.resize(initialPathStates.size());
        if (!result.nextPathStates.empty()) {
          std::memcpy(result.nextPathStates.data(), [nextPathBuffer contents],
                      result.nextPathStates.size() * sizeof(GpuDiffusePathStateRecord));
        }
        result.closestHitRecords.resize(initialPathStates.size());
        if (!result.closestHitRecords.empty()) {
          std::memcpy(result.closestHitRecords.data(), [closestHitBuffer contents],
                      result.closestHitRecords.size() * sizeof(GpuIntersectionHitRecord));
        }

        const std::size_t rawStepCount =
          initialPathStates.size() * static_cast<std::size_t>(plan.parameters.maxDepth);
        std::vector<GpuDiffusePathStepRecord> rawStepRecords(rawStepCount);
        if (!rawStepRecords.empty()) {
          std::memcpy(rawStepRecords.data(), [stepRecordBuffer contents],
                      rawStepRecords.size() * sizeof(GpuDiffusePathStepRecord));
        }
        result.stepRecords.reserve(rawStepRecords.size());
        for (const GpuDiffusePathStepRecord& step : rawStepRecords) {
          if (static_cast<GpuDiffusePathStepEvent>(step.event) !=
              GpuDiffusePathStepEvent::Inactive) {
            result.stepRecords.push_back(step);
          }
        }

        result.retainedPathIndices =
          retainedPathIndicesFromBuffer(retainedIndexBuffer, initialPathStates.size());
      }
      result.accumulationColorSums.resize(pixelCount(plan.parameters));
      if (!result.accumulationColorSums.empty()) {
        std::memcpy(result.accumulationColorSums.data(), [accumulationBuffer contents],
                    result.accumulationColorSums.size() * sizeof(std::array<float, 4>));
      }
      result.accumulationSampleCounts.resize(pixelCount(plan.parameters));
      if (!result.accumulationSampleCounts.empty()) {
        const std::uint64_t colorBytes =
          result.accumulationColorSums.size() * sizeof(std::array<float, 4>);
        const auto* sampleCountBytes =
          static_cast<const std::uint8_t*>([accumulationBuffer contents]) + colorBytes;
        std::memcpy(result.accumulationSampleCounts.data(), sampleCountBytes,
                    result.accumulationSampleCounts.size() * sizeof(std::uint32_t));
      }
      result.readbackWorkerSeconds =
        elapsedSeconds(readbackStart, std::chrono::steady_clock::now());
      return result;
    }
  }
}
