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
    static_assert(sizeof(GpuDiffusePathLoopLaunchParameters) == 160);
    static_assert(alignof(GpuDiffusePathLoopLaunchParameters) == 16);
    static_assert(sizeof(GpuDiffusePathStateRecord) == 160);
    static_assert(alignof(GpuDiffusePathStateRecord) == 16);
    static_assert(sizeof(GpuDiffusePathStepRecord) == 96);
    static_assert(alignof(GpuDiffusePathStepRecord) == 16);
    static_assert(sizeof(GpuIntersectionHitRecord) == 112);
    static_assert(alignof(GpuIntersectionHitRecord) == 16);
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
              "  uint geometryByteOffset;\n"
              "  uint materialByteOffset;\n"
              "  uint textureByteOffset;\n"
              "  uint lightByteOffset;\n"
              "  uint environmentByteOffset;\n"
              "  uint debugIdByteOffset;\n"
              "  uint sceneUploadBytes;\n"
              "  uint reserved0;\n"
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
              "struct GpuIntersectionSpherePayload {\n"
              "  float4 centerRadius;\n"
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
              "struct LocalSphereHit {\n"
              "  bool hit;\n"
              "  float distance;\n"
              "  float4 point;\n"
              "  float4 normal;\n"
              "};\n"
              "constant uint gpuIntersectionLeafNodeFlag = 1u;\n"
              "constant uint gpuIntersectionSpherePrimitiveKind = 2u;\n"
              "constant uint gpuDiffusePathStateActiveFlag = 1u;\n"
              "constant uint gpuDiffusePathStateTerminatedFlag = 2u;\n"
              "constant uint gpuDiffusePathStepEventInactive = 0u;\n"
              "constant uint gpuDiffusePathStepEventMiss = 1u;\n"
              "constant uint gpuDiffusePathStepEventHit = 2u;\n"
              "float finiteInfinity() {\n"
              "  return 3.4028234663852886e+38f;\n"
              "}\n"
              "bool pathStateIsActive(const GpuDiffusePathStateRecord path) {\n"
              "  return (path.flags & gpuDiffusePathStateActiveFlag) != 0u &&\n"
              "         (path.flags & gpuDiffusePathStateTerminatedFlag) == 0u;\n"
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
              "LocalSphereHit intersectSphere(const GpuIntersectionRay ray,\n"
              "                               const GpuIntersectionSpherePayload sphere,\n"
              "                               float maxHitDistance) {\n"
              "  LocalSphereHit result;\n"
              "  result.hit = false;\n"
              "  result.distance = maxHitDistance;\n"
              "  result.point = float4(0.0f);\n"
              "  result.normal = float4(0.0f);\n"
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
              "GpuIntersectionHitRecord hitRecordForSphere(\n"
              "    const GpuIntersectionRay ray,\n"
              "    const GpuIntersectionPrimitiveRecord primitive,\n"
              "    uint primitiveRecordIndex,\n"
              "    const LocalSphereHit sphereHit) {\n"
              "  GpuIntersectionHitRecord hit = missHitRecord(ray);\n"
              "  hit.hit = 1u;\n"
              "  hit.material = primitive.material;\n"
              "  hit.object = primitive.object;\n"
              "  hit.primitiveRecord = primitiveRecordIndex;\n"
              "  hit.distance = sphereHit.distance;\n"
              "  hit.point = sphereHit.point;\n"
              "  hit.normal = sphereHit.normal;\n"
              "  return hit;\n"
              "}\n"
              "GpuIntersectionHitRecord closestSphereHit(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    const GpuIntersectionRay ray) {\n"
              "  GpuIntersectionHitRecord closest = missHitRecord(ray);\n"
              "  if (parameters.bvhNodeCount == 0u || parameters.primitiveCount == 0u ||\n"
              "      parameters.sphereCount == 0u) {\n"
              "    return closest;\n"
              "  }\n"
              "  device const GpuIntersectionBvhNode* bvh =\n"
              "      reinterpret_cast<device const GpuIntersectionBvhNode*>(\n"
              "          sceneUpload + parameters.bvhByteOffset);\n"
              "  device const GpuIntersectionPrimitiveRecord* primitives =\n"
              "      reinterpret_cast<device const GpuIntersectionPrimitiveRecord*>(\n"
              "          sceneUpload + parameters.primitiveByteOffset);\n"
              "  device const GpuIntersectionSpherePayload* spheres =\n"
              "      reinterpret_cast<device const GpuIntersectionSpherePayload*>(\n"
              "          sceneUpload + parameters.sphereByteOffset);\n"
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
              "      if (primitive.kind != gpuIntersectionSpherePrimitiveKind ||\n"
              "          primitive.payloadOffset >= parameters.sphereCount ||\n"
              "          !boundsIntersectsRay(primitive.bounds, ray, closest.distance)) {\n"
              "        continue;\n"
              "      }\n"
              "      const LocalSphereHit sphereHit = intersectSphere(\n"
              "          ray, spheres[primitive.payloadOffset], closest.distance);\n"
              "      if (sphereHit.hit) {\n"
              "        closest = hitRecordForSphere(ray, primitive, primitiveIndex, sphereHit);\n"
              "      }\n"
              "    }\n"
              "  }\n"
              "  return closest;\n"
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
              "float4 missRadiance(constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "                    device const uchar* sceneUpload,\n"
              "                    GpuDiffusePathStateRecord path) {\n"
              "  if (parameters.environmentCount == 0u) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  uint environmentIndex = path.depth == 0u ? 0u : parameters.environmentCount - 1u;\n"
              "  device const GpuTracingEnvironmentRecord* environment =\n"
              "      reinterpret_cast<device const GpuTracingEnvironmentRecord*>(\n"
              "          sceneUpload + parameters.environmentByteOffset);\n"
              "  return environment[environmentIndex].color;\n"
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
              "    device uchar* retainedIndices [[buffer(7)]],\n"
              "    device uchar* accumulation [[buffer(8)]],\n"
              "    uint id [[thread_position_in_grid]]) {\n"
              "  if (id == 0u) {\n"
              "    echoedParameters[0] = parameters;\n"
              "    retainedIndices[0] = 0u;\n"
              "  }\n"
              "  if (id >= parameters.initialPathCount) {\n"
              "    return;\n"
              "  }\n"
              "  activePathStates[id] = initialPathStates[id];\n"
              "  nextPathStates[id] = activePathStates[id];\n"
              "  stepRecords[id] = inactiveStep(id, initialPathStates[id]);\n"
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
              "    device uchar* retainedIndices [[buffer(7)]],\n"
              "    device uchar* accumulation [[buffer(8)]],\n"
              "    uint id [[thread_position_in_grid]]) {\n"
              "  if (id == 0u) {\n"
              "    echoedParameters[0] = parameters;\n"
              "    retainedIndices[0] = 0u;\n"
              "  }\n"
              "  if (id >= parameters.initialPathCount) {\n"
              "    return;\n"
              "  }\n"
              "  GpuDiffusePathStateRecord path = initialPathStates[id];\n"
              "  GpuDiffusePathStepRecord step = inactiveStep(id, path);\n"
              "  if (pathStateIsActive(path)) {\n"
              "    float4 contribution = path.throughput * missRadiance(parameters, sceneUpload, path);\n"
              "    path.accumulatedRadiance += contribution;\n"
              "    path.flags = (path.flags & ~gpuDiffusePathStateActiveFlag) |\n"
              "                 gpuDiffusePathStateTerminatedFlag;\n"
              "    accumulationColorSums(parameters, accumulation)[path.pixelIndex] = contribution;\n"
              "    accumulationSampleCounts(parameters, accumulation)[path.pixelIndex] = 1u;\n"
              "    step.event = gpuDiffusePathStepEventMiss;\n"
              "    step.missRadiance = contribution;\n"
              "    step.flags = path.flags;\n"
              "  }\n"
              "  activePathStates[id] = path;\n"
              "  nextPathStates[id] = path;\n"
              "  stepRecords[id] = step;\n"
              "}\n"
              "kernel void probeDiffusePathLoopClosestHit(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters [[buffer(0)]],\n"
              "    device GpuDiffusePathLoopLaunchParameters* echoedParameters [[buffer(1)]],\n"
              "    device const uchar* sceneUpload [[buffer(2)]],\n"
              "    device const GpuDiffusePathStateRecord* initialPathStates [[buffer(3)]],\n"
              "    device GpuDiffusePathStateRecord* activePathStates [[buffer(4)]],\n"
              "    device GpuDiffusePathStateRecord* nextPathStates [[buffer(5)]],\n"
              "    device GpuDiffusePathStepRecord* stepRecords [[buffer(6)]],\n"
              "    device uchar* retainedIndices [[buffer(7)]],\n"
              "    device uchar* accumulation [[buffer(8)]],\n"
              "    device GpuIntersectionHitRecord* closestHits [[buffer(9)]],\n"
              "    uint id [[thread_position_in_grid]]) {\n"
              "  if (id == 0u) {\n"
              "    echoedParameters[0] = parameters;\n"
              "    retainedIndices[0] = 0u;\n"
              "  }\n"
              "  if (id >= parameters.initialPathCount) {\n"
              "    return;\n"
              "  }\n"
              "  GpuDiffusePathStateRecord path = initialPathStates[id];\n"
              "  GpuDiffusePathStepRecord step = inactiveStep(id, path);\n"
              "  GpuIntersectionHitRecord hit = missHitRecord(path.ray);\n"
              "  if (pathStateIsActive(path)) {\n"
              "    hit = closestSphereHit(parameters, sceneUpload, path.ray);\n"
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
              "  (void)accumulation;\n"
              "}\n";
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

    void validateAllMissAccumulationTargets(
      const GpuDiffusePathLoopLaunchPlan& plan,
      const std::vector<GpuDiffusePathStateRecord>& initialPathStates) {
      const std::uint64_t pixels = pixelCount(plan.parameters);
      if (pixels > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::overflow_error("Metal diffuse path-loop all-miss pixel count overflows");
      }
      std::vector<bool> seenPixels(static_cast<std::size_t>(pixels), false);
      for (const GpuDiffusePathStateRecord& path : initialPathStates) {
        if (!gpuDiffusePathStateIsActive(path)) {
          continue;
        }
        if (path.pixelIndex >= pixels) {
          throw std::invalid_argument(
            "Metal diffuse path-loop all-miss path pixel is outside the accumulation layout");
        }
        if (seenPixels[path.pixelIndex]) {
          throw std::invalid_argument(
            "Metal diffuse path-loop all-miss probe requires unique active pixel targets");
        }
        seenPixels[path.pixelIndex] = true;
      }
    }

    void validateClosestHitProbeGeometry(const GpuDiffusePathLoopLaunchPlan& plan) {
      const GpuDiffusePathLoopLaunchParameters& parameters = plan.parameters;
      if (parameters.primitiveCount == 0u || parameters.bvhNodeCount == 0u) {
        throw std::invalid_argument(
          "Metal diffuse path-loop closest-hit probe requires compiled sphere geometry");
      }
      if (parameters.triangleCount != 0u || parameters.planeCount != 0u ||
          parameters.rectangleCount != 0u || parameters.diskCount != 0u ||
          parameters.openCylinderCount != 0u || parameters.torusCount != 0u ||
          parameters.transformCount != 0u) {
        throw std::invalid_argument(
          "Metal diffuse path-loop closest-hit probe currently supports only untransformed "
          "sphere geometry");
      }
      if (parameters.sphereCount != parameters.primitiveCount) {
        throw std::invalid_argument(
          "Metal diffuse path-loop closest-hit probe requires one sphere payload per primitive");
      }
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
      id<MTLBuffer> initialPathBuffer = initialPathStates.empty()
                                          ? [device newBufferWithLength:1
                                                                options:MTLResourceStorageModeShared]
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
      [encoder dispatchThreads:MTLSizeMake(std::max<std::size_t>(1, initialPathStates.size()), 1, 1)
         threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
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
    validateAllMissAccumulationTargets(plan, initialPathStates);

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
      id<MTLBuffer> initialPathBuffer = initialPathStates.empty()
                                          ? [device newBufferWithLength:1
                                                                options:MTLResourceStorageModeShared]
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
      [encoder dispatchThreads:MTLSizeMake(std::max<NSUInteger>(1, pixels), 1, 1)
         threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];

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
      [encoder dispatchThreads:MTLSizeMake(std::max<std::size_t>(1, initialPathStates.size()), 1, 1)
         threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
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
      id<MTLBuffer> initialPathBuffer = initialPathStates.empty()
                                          ? [device newBufferWithLength:1
                                                                options:MTLResourceStorageModeShared]
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
      [encoder dispatchThreads:MTLSizeMake(std::max<std::size_t>(1, initialPathStates.size()), 1, 1)
         threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
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
      result.readbackWorkerSeconds =
        elapsedSeconds(readbackStart, std::chrono::steady_clock::now());
      return result;
    }
  }
}
