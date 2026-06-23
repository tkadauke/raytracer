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
    static_assert(sizeof(GpuTracingMaterialRecord) == 48);
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
              "struct GpuTracingMaterialRecord {\n"
              "  uint kind;\n"
              "  uint albedoTexture;\n"
              "  uint emissionTexture;\n"
              "  uint flags;\n"
              "  float4 parameters;\n"
              "  float4 continuationParameters;\n"
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
              "constant uint gpuTracingMatteMaterialKind = 1u;\n"
              "constant uint gpuTracingConstantColorTextureKind = 1u;\n"
              "constant uint gpuTracingPointLightKind = 1u;\n"
              "constant uint gpuTracingDirectionalLightKind = 2u;\n"
              "constant uint gpuDiffusePathStateActiveFlag = 1u;\n"
              "constant uint gpuDiffusePathStateTerminatedFlag = 2u;\n"
              "constant uint gpuDiffusePathStateSampledFromBsdfFlag = 4u;\n"
              "constant uint gpuDiffusePathStateUnsupportedFlag = 16u;\n"
              "constant uint gpuDiffusePathStepEventInactive = 0u;\n"
              "constant uint gpuDiffusePathStepEventMiss = 1u;\n"
              "constant uint gpuDiffusePathStepEventHit = 2u;\n"
              "constant uint gpuDiffusePathStepEventUnsupported = 3u;\n"
              "constant uint gpuSampleInitialCoordinateState = 0x811c9dc5u;\n"
              "constant uint gpuSampleCoordinateStep = 0x9e3779b9u;\n"
              "constant float pathLoopInvPi = 0.31830988618379067154f;\n"
              "constant float pathLoopTau = 6.28318530717958647692f;\n"
              "constant float pathLoopRayEpsilon = 1.0e-7f;\n"
              "constant float pathLoopMinimumContinuationProbability = 0.05f;\n"
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
              "float4 matteDiffuseReflectance(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    const GpuIntersectionHitRecord hit,\n"
              "    thread bool& supported) {\n"
              "  supported = false;\n"
              "  if (hit.material >= parameters.materialCount) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  device const GpuTracingMaterialRecord* materials =\n"
              "      reinterpret_cast<device const GpuTracingMaterialRecord*>(\n"
              "          sceneUpload + parameters.materialByteOffset);\n"
              "  const GpuTracingMaterialRecord material = materials[hit.material];\n"
              "  if (material.kind != gpuTracingMatteMaterialKind ||\n"
              "      material.albedoTexture >= parameters.textureCount) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  device const GpuTracingTextureRecord* textures =\n"
              "      reinterpret_cast<device const GpuTracingTextureRecord*>(\n"
              "          sceneUpload + parameters.textureByteOffset);\n"
              "  const GpuTracingTextureRecord albedo = textures[material.albedoTexture];\n"
              "  if (albedo.kind != gpuTracingConstantColorTextureKind) {\n"
              "    return float4(0.0f);\n"
              "  }\n"
              "  supported = true;\n"
              "  return albedo.parameters * material.parameters.y;\n"
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
              "float compiledLightSelectionWeight(GpuTracingLightRecord light) {\n"
              "  if (light.kind == gpuTracingPointLightKind ||\n"
              "      light.kind == gpuTracingDirectionalLightKind) {\n"
              "    return maxColor(light.parameters);\n"
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
              "  if (totalWeight <= 0.0f) {\n"
              "    return selection;\n"
              "  }\n"
              "  const float unitSample = clamp(\n"
              "      sample1D(path, lightSelectionDimension(path, directSampleIndex), 0u),\n"
              "      0.0f, 0.9999999403953552f);\n"
              "  const float target = unitSample * totalWeight;\n"
              "  float cumulative = 0.0f;\n"
              "  for (uint lightIndex = 0u; lightIndex != parameters.lightCount; ++lightIndex) {\n"
              "    const float weight = compiledLightSelectionWeight(lights[lightIndex]);\n"
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
              "DirectLightSample sampleDirectLight(GpuTracingLightRecord light, float3 point) {\n"
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
              "  return sample;\n"
              "}\n"
              "GpuIntersectionRay shadowRayFor(float3 point, DirectLightSample sample) {\n"
              "  GpuIntersectionRay ray;\n"
              "  ray.origin = float4(point + sample.direction * pathLoopRayEpsilon, 1.0f);\n"
              "  ray.direction = float4(sample.direction, 0.0f);\n"
              "  ray.minDistance = 0.0f;\n"
              "  ray.maxDistance = sample.distance;\n"
              "  ray.timeSample = 0.0f;\n"
              "  ray.flags = 0u;\n"
              "  ray.rayIndex = 0u;\n"
              "  ray.reserved0 = 0u;\n"
              "  ray.reserved1 = 0u;\n"
              "  ray.reserved2 = 0u;\n"
              "  return ray;\n"
              "}\n"
              "float4 directLightRadiance(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    device const uchar* sceneUpload,\n"
              "    const GpuIntersectionHitRecord hit,\n"
              "    const GpuDiffusePathStateRecord path,\n"
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
              "  const float4 bsdfValue = matteReflectance * pathLoopInvPi;\n"
              "  float4 radiance = float4(0.0f);\n"
              "  for (uint sampleIndex = 0u; sampleIndex != sampleCount; ++sampleIndex) {\n"
              "    const DirectLightSelection selection = selectDirectLight(\n"
              "        parameters, sceneUpload, path, sampleIndex);\n"
              "    if (selection.valid == 0u || selection.lightIndex >= parameters.lightCount ||\n"
              "        selection.pdf <= 0.0f) {\n"
              "      continue;\n"
              "    }\n"
              "    const DirectLightSample light = sampleDirectLight(lights[selection.lightIndex], point);\n"
              "    const float normalDotLight = dot(normal, light.direction);\n"
              "    if (light.valid == 0u || light.pdf <= 0.0f || normalDotLight <= 0.0f) {\n"
              "      continue;\n"
              "    }\n"
              "    const GpuIntersectionRay visibilityRay = shadowRayFor(point, light);\n"
              "    if (closestSphereHit(parameters, sceneUpload, visibilityRay).hit != 0u) {\n"
              "      continue;\n"
              "    }\n"
              "    const float4 contribution = path.throughput * bsdfValue * light.radiance *\n"
              "                                (normalDotLight / (light.pdf * selection.pdf));\n"
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
              "  ray.minDistance = 0.0f;\n"
              "  ray.maxDistance = rayInfinity();\n"
              "  ray.timeSample = 0.0f;\n"
              "  return ray;\n"
              "}\n"
              "GpuDiffusePathStateRecord matteContinuationPath(\n"
              "    constant GpuDiffusePathLoopLaunchParameters& parameters,\n"
              "    const GpuIntersectionHitRecord hit,\n"
              "    const GpuDiffusePathStateRecord path,\n"
              "    float4 continuationThroughput,\n"
              "    float4 accumulatedRadiance,\n"
              "    thread bool& spawned) {\n"
              "  spawned = false;\n"
              "  GpuDiffusePathStateRecord next = path;\n"
              "  next.accumulatedRadiance = accumulatedRadiance;\n"
              "  const float3 normal = normalize(hit.normal.xyz);\n"
              "  const float2 bsdfSample = sample2D(path, sampleDimension(path, 0u));\n"
              "  const float3 direction = cosineHemisphereDirection(normal, bsdfSample);\n"
              "  const float pdf = cosineHemispherePdf(normal, direction);\n"
              "  if (pdf <= 0.0f) {\n"
              "    continuationThroughput = float4(0.0f);\n"
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
              "  next.previousLightPdf = 0.0f;\n"
              "  next.previousMaterial = hit.material;\n"
              "  next.previousEventFlags = gpuDiffusePathStateSampledFromBsdfFlag;\n"
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
              "}\n"
              "kernel void probeDiffusePathLoopMatteContinuation(\n"
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
              "  GpuDiffusePathStateRecord next = path;\n"
              "  GpuDiffusePathStepRecord step = inactiveStep(id, path);\n"
              "  GpuIntersectionHitRecord hit = missHitRecord(path.ray);\n"
              "  if (pathStateIsActive(path)) {\n"
              "    hit = closestSphereHit(parameters, sceneUpload, path.ray);\n"
              "    if (hit.hit != 0u) {\n"
              "      bool supported = false;\n"
              "      const float4 reflectance = matteDiffuseReflectance(\n"
              "          parameters, sceneUpload, hit, supported);\n"
              "      const float4 continuation = path.throughput * reflectance;\n"
              "      step.event = supported ? gpuDiffusePathStepEventHit :\n"
              "                              gpuDiffusePathStepEventUnsupported;\n"
              "      step.material = hit.material;\n"
              "      step.object = hit.object;\n"
              "      if (supported) {\n"
              "        const float4 directLight = directLightRadiance(\n"
              "            parameters, sceneUpload, hit, path, reflectance);\n"
              "        const float4 accumulatedRadiance = path.accumulatedRadiance + directLight;\n"
              "        step.directLightRadiance = directLight;\n"
              "        bool spawned = false;\n"
              "        next = matteContinuationPath(parameters, hit, path, continuation,\n"
              "                                     accumulatedRadiance, spawned);\n"
              "        step.continuationThroughput = spawned ? next.throughput : float4(0.0f);\n"
              "        step.flags = next.flags;\n"
              "      } else {\n"
              "        next.flags = unsupportedPathFlags(path.flags);\n"
              "        step.flags = next.flags;\n"
              "      }\n"
              "    } else {\n"
              "      step.event = gpuDiffusePathStepEventMiss;\n"
              "      step.flags = path.flags;\n"
              "    }\n"
              "  }\n"
              "  activePathStates[id] = path;\n"
              "  nextPathStates[id] = next;\n"
              "  stepRecords[id] = step;\n"
              "  closestHits[id] = hit;\n"
              "  (void)accumulation;\n"
              "}\n"
              "kernel void probeDiffusePathLoopMatteHitShading(\n"
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

    void validateMatteHitShadingProbeScene(const GpuDiffusePathLoopLaunchPlan& plan) {
      validateClosestHitProbeGeometry(plan);
      if (plan.parameters.materialCount == 0u || plan.parameters.textureCount == 0u) {
        throw std::invalid_argument(
          "Metal diffuse path-loop matte shading probe requires material and texture records");
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
          "Metal diffuse path-loop matte shading probe buffer allocation failed");
      }
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
      [encoder dispatchThreads:MTLSizeMake(std::max<std::size_t>(1, initialPathStates.size()), 1, 1)
         threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
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

    @autoreleasepool {
      if (!launchPathAvailable()) {
        throw std::runtime_error(launchPathUnavailableReason());
      }

      id<MTLDevice> device = sharedMetalDevice();
      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLComputePipelineState> pipeline = sharedMatteContinuationProbePipeline();
      if (!pipeline) {
        throw std::runtime_error(
          "Metal diffuse path-loop matte continuation probe pipeline was not created");
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
          "Metal diffuse path-loop matte continuation probe buffer allocation failed");
      }
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
      result.readbackWorkerSeconds =
        elapsedSeconds(readbackStart, std::chrono::steady_clock::now());
      return result;
    }
  }
}
