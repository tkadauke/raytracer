#include "render/MetalWavefrontSmokeKernel.h"

#include "render/GpuIntersectionScene.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace render {
  namespace {
    double secondsBetween(std::chrono::steady_clock::time_point start,
                          std::chrono::steady_clock::time_point end) {
      return std::chrono::duration<double>(end - start).count();
    }

    NSString* smokeKernelSource() {
      return @"#include <metal_stdlib>\n"
              "using namespace metal;\n"
              "kernel void wavefrontSmokeKernel(device const uint* rayIds [[buffer(0)]],\n"
              "                                  device uint* results [[buffer(1)]],\n"
              "                                  uint id [[thread_position_in_grid]]) {\n"
              "  results[id] = rayIds[id] ^ 0xa5a5a5a5u;\n"
              "}\n";
    }

    NSString* rayCompactionKernelSource() {
      return @"#include <metal_stdlib>\n"
              "using namespace metal;\n"
              "struct RayRecord {\n"
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
              "kernel void compactRayBatch(device const RayRecord* sourceRays [[buffer(0)]],\n"
              "                            device const uint* retainedRayIndices [[buffer(1)]],\n"
              "                            device RayRecord* compactedRays [[buffer(2)]],\n"
              "                            uint id [[thread_position_in_grid]]) {\n"
              "  compactedRays[id] = sourceRays[retainedRayIndices[id]];\n"
              "}\n";
    }

    NSString* basicHitKernelSource() {
      return @"#include <metal_stdlib>\n"
              "using namespace metal;\n"
              "constant uint leafNodeFlag = 1u;\n"
              "constant uint triangleKind = 1u;\n"
              "constant uint sphereKind = 2u;\n"
              "constant uint planeKind = 3u;\n"
              "constant uint rectangleKind = 4u;\n"
              "constant uint diskKind = 5u;\n"
              "constant uint openCylinderKind = 6u;\n"
              "constant uint torusKind = 7u;\n"
              "constant float kernelEpsilon = 1.1920928955078125e-7f;\n"
              "constant float rayOcclusionEpsilon = 4.0e-7f;\n"
              "float positiveInfinity() {\n"
              "  return as_type<float>(0x7f800000u);\n"
              "}\n"
              "struct Bounds {\n"
              "  float4 minimum;\n"
              "  float4 maximum;\n"
              "};\n"
              "struct BvhNode {\n"
              "  Bounds bounds;\n"
              "  uint leftOrFirstPrimitive;\n"
              "  uint primitiveCount;\n"
              "  uint flags;\n"
              "  uint reserved;\n"
              "};\n"
              "struct PrimitiveRecord {\n"
              "  Bounds bounds;\n"
              "  uint kind;\n"
              "  uint material;\n"
              "  uint object;\n"
              "  uint transform;\n"
              "  uint payloadOffset;\n"
              "  uint payloadCount;\n"
              "  uint reserved0;\n"
              "  uint reserved1;\n"
              "};\n"
              "struct TrianglePayload {\n"
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
              "struct SpherePayload {\n"
              "  float4 centerRadius;\n"
              "};\n"
              "struct PlanePayload {\n"
              "  float4 normalDistance;\n"
              "};\n"
              "struct RectanglePayload {\n"
              "  float4 corner;\n"
              "  float4 leg1;\n"
              "  float4 leg2;\n"
              "  float4 normal;\n"
              "};\n"
              "struct DiskPayload {\n"
              "  float4 centerRadius;\n"
              "  float4 normalMinimumHitDistance;\n"
              "};\n"
              "struct OpenCylinderPayload {\n"
              "  float4 radiusHalfHeight;\n"
              "};\n"
              "struct TorusPayload {\n"
              "  float4 sweptTubeRadius;\n"
              "};\n"
              "struct TransformPayload {\n"
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
              "struct RayRecord {\n"
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
              "struct HitRecord {\n"
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
              "struct OcclusionRecord {\n"
              "  uint occluded;\n"
              "  uint rayIndex;\n"
              "  uint reserved0;\n"
              "  uint reserved1;\n"
              "};\n"
              "struct LocalHit {\n"
              "  bool hit;\n"
              "  float distance;\n"
              "  float4 point;\n"
              "  float4 normal;\n"
              "  float4 uv;\n"
              "  float4 barycentric;\n"
              "};\n"
              "LocalHit makeLocalMiss() {\n"
              "  LocalHit result;\n"
              "  result.hit = false;\n"
              "  result.distance = positiveInfinity();\n"
              "  result.point = float4(0.0f);\n"
              "  result.normal = float4(0.0f);\n"
              "  result.uv = float4(0.0f);\n"
              "  result.barycentric = float4(0.0f);\n"
              "  return result;\n"
              "}\n"
              "float boundsEntryDistance(Bounds bounds, RayRecord ray, float maxHitDistance) {\n"
              "  float enter = ray.minDistance;\n"
              "  float exit = min(ray.maxDistance, maxHitDistance);\n"
              "  if (exit < enter) {\n"
              "    return positiveInfinity();\n"
              "  }\n"
              "  for (uint axis = 0u; axis != 3u; ++axis) {\n"
              "    const float origin = ray.origin[axis];\n"
              "    const float direction = ray.direction[axis];\n"
              "    const float minimum = bounds.minimum[axis];\n"
              "    const float maximum = bounds.maximum[axis];\n"
              "    if (abs(direction) <= kernelEpsilon) {\n"
              "      if (origin < minimum || origin > maximum) {\n"
              "        return positiveInfinity();\n"
              "      }\n"
              "      continue;\n"
              "    }\n"
              "    const float inverseDirection = 1.0f / direction;\n"
              "    float nearDistance = (minimum - origin) * inverseDirection;\n"
              "    float farDistance = (maximum - origin) * inverseDirection;\n"
              "    if (nearDistance > farDistance) {\n"
              "      const float temporary = nearDistance;\n"
              "      nearDistance = farDistance;\n"
              "      farDistance = temporary;\n"
              "    }\n"
              "    enter = max(enter, nearDistance);\n"
              "    exit = min(exit, farDistance);\n"
              "    if (exit < enter) {\n"
              "      return positiveInfinity();\n"
              "    }\n"
              "  }\n"
              "  return enter;\n"
              "}\n"
              "bool boundsIntersect(Bounds bounds, RayRecord ray, float maxHitDistance) {\n"
              "  return boundsEntryDistance(bounds, ray, maxHitDistance) < positiveInfinity();\n"
              "}\n"
              "float4 normalize3(float4 value) {\n"
              "  const float lengthSquared = dot(value.xyz, value.xyz);\n"
              "  if (lengthSquared <= kernelEpsilon) {\n"
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
              "RayRecord transformRay(RayRecord ray, TransformPayload transform) {\n"
              "  RayRecord result = ray;\n"
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
              "LocalHit transformHit(LocalHit hit, TransformPayload transform) {\n"
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
              "float4 interpolate3(float4 a, float4 b, float4 c, float alpha, float beta,\n"
              "                    float gamma) {\n"
              "  return a * alpha + b * beta + c * gamma;\n"
              "}\n"
              "LocalHit intersectTriangle(RayRecord ray, TrianglePayload triangle) {\n"
              "  LocalHit result = makeLocalMiss();\n"
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
              "  const float invDenom = 1.0f / denominator;\n"
              "  const float beta = (d * m - b * n - c * p) * invDenom;\n"
              "  if (beta < 0.0f || beta > 1.0f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float gamma = (a * n + d * q + c * r) * invDenom;\n"
              "  if (gamma < 0.0f || gamma > 1.0f || beta + gamma > 1.0f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float distance = (a * p - b * r + d * s) * invDenom;\n"
              "  const float minimumDistance = max(ray.minDistance,\n"
              "                                    triangle.minimumHitDistance.x);\n"
              "  if (distance < minimumDistance || distance > ray.maxDistance) {\n"
              "    return result;\n"
              "  }\n"
              "  const float alpha = 1.0f - beta - gamma;\n"
              "  result.hit = true;\n"
              "  result.distance = distance;\n"
              "  result.point = ray.origin + ray.direction * distance;\n"
              "  result.point.w = 1.0f;\n"
              "  result.normal = normalize3(interpolate3(triangle.normal0, triangle.normal1,\n"
              "                                      triangle.normal2, alpha, beta, gamma));\n"
              "  result.uv = interpolate3(triangle.uv0, triangle.uv1, triangle.uv2, alpha,\n"
              "                           beta, gamma);\n"
              "  result.barycentric = float4(alpha, beta, gamma, 0.0f);\n"
              "  return result;\n"
              "}\n"
              "LocalHit intersectSphere(RayRecord ray, SpherePayload sphere) {\n"
              "  LocalHit result = makeLocalMiss();\n"
              "  const float3 center = sphere.centerRadius.xyz;\n"
              "  const float radius = sphere.centerRadius.w;\n"
              "  const float3 origin = ray.origin.xyz - center;\n"
              "  const float3 direction = ray.direction.xyz;\n"
              "  const float od = dot(origin, direction);\n"
              "  const float dd = dot(direction, direction);\n"
              "  if (dd <= kernelEpsilon) {\n"
              "    return result;\n"
              "  }\n"
              "  const float originLengthSquared = dot(origin, origin);\n"
              "  const float discriminant = od * od - dd * (originLengthSquared - radius * radius);\n"
              "  if (discriminant <= 0.0f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float discriminantRoot = sqrt(discriminant);\n"
              "  const float nearDistance = (-od - discriminantRoot) / dd;\n"
              "  const float farDistance = (-od + discriminantRoot) / dd;\n"
              "  if (nearDistance <= 0.0f && farDistance <= 0.0f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float distance = nearDistance >= ray.minDistance ? nearDistance : farDistance;\n"
              "  if (distance < ray.minDistance || distance > ray.maxDistance) {\n"
              "    return result;\n"
              "  }\n"
              "  result.hit = true;\n"
              "  result.distance = distance;\n"
              "  result.point = ray.origin + ray.direction * distance;\n"
              "  result.point.w = 1.0f;\n"
              "  result.normal = normalize3(float4(result.point.xyz - center, 0.0f));\n"
              "  return result;\n"
              "}\n"
              "LocalHit intersectPlane(RayRecord ray, PlanePayload plane) {\n"
              "  LocalHit result = makeLocalMiss();\n"
              "  const float3 normal = plane.normalDistance.xyz;\n"
              "  const float angle = dot(normal, ray.direction.xyz);\n"
              "  if (angle == 0.0f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float distance = -(dot(normal, ray.origin.xyz) + plane.normalDistance.w) /\n"
              "                         angle;\n"
              "  if (distance <= 0.0f || distance < ray.minDistance || distance > ray.maxDistance) {\n"
              "    return result;\n"
              "  }\n"
              "  result.hit = true;\n"
              "  result.distance = distance;\n"
              "  result.point = ray.origin + ray.direction * distance;\n"
              "  result.point.w = 1.0f;\n"
              "  result.normal = float4(normal, 0.0f);\n"
              "  return result;\n"
              "}\n"
              "LocalHit intersectRectangle(RayRecord ray, RectanglePayload rectangle) {\n"
              "  LocalHit result = makeLocalMiss();\n"
              "  const float3 normal = rectangle.normal.xyz;\n"
              "  const float denominator = dot(ray.direction.xyz, normal);\n"
              "  if (denominator == 0.0f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float3 cornerToOrigin = rectangle.corner.xyz - ray.origin.xyz;\n"
              "  const float distance = dot(cornerToOrigin, normal) / denominator;\n"
              "  if (!isfinite(distance) || distance < 0.0f || distance < ray.minDistance ||\n"
              "      distance > ray.maxDistance) {\n"
              "    return result;\n"
              "  }\n"
              "  const float4 point = ray.origin + ray.direction * distance;\n"
              "  const float3 difference = point.xyz - rectangle.corner.xyz;\n"
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
              "  result.point = point;\n"
              "  result.point.w = 1.0f;\n"
              "  result.normal = float4(normal, 0.0f);\n"
              "  return result;\n"
              "}\n"
              "LocalHit intersectDisk(RayRecord ray, DiskPayload disk) {\n"
              "  LocalHit result = makeLocalMiss();\n"
              "  const float3 center = disk.centerRadius.xyz;\n"
              "  const float radius = disk.centerRadius.w;\n"
              "  const float3 normal = disk.normalMinimumHitDistance.xyz;\n"
              "  const float minimumHitDistance = disk.normalMinimumHitDistance.w;\n"
              "  const float denominator = dot(ray.direction.xyz, normal);\n"
              "  if (denominator == 0.0f) {\n"
              "    return result;\n"
              "  }\n"
              "  const float distance = dot(center - ray.origin.xyz, normal) / denominator;\n"
              "  if (!isfinite(distance) || distance < minimumHitDistance ||\n"
              "      distance < ray.minDistance ||\n"
              "      distance > ray.maxDistance) {\n"
              "    return result;\n"
              "  }\n"
              "  const float4 point = ray.origin + ray.direction * distance;\n"
              "  const float3 hitOffset = point.xyz - center;\n"
              "  const float squaredDistance = dot(hitOffset, hitOffset);\n"
              "  if (!(squaredDistance < radius * radius)) {\n"
              "    return result;\n"
              "  }\n"
              "  result.hit = true;\n"
              "  result.distance = distance;\n"
              "  result.point = point;\n"
              "  result.point.w = 1.0f;\n"
              "  result.normal = float4(normal, 0.0f);\n"
              "  return result;\n"
              "}\n"
              "LocalHit intersectOpenCylinder(RayRecord ray, OpenCylinderPayload openCylinder) {\n"
              "  LocalHit result = makeLocalMiss();\n"
              "  const float radius = openCylinder.radiusHalfHeight.x;\n"
              "  const float halfHeight = openCylinder.radiusHalfHeight.y;\n"
              "  const float inverseRadius = openCylinder.radiusHalfHeight.z;\n"
              "  const float a = ray.direction.x * ray.direction.x +\n"
              "                  ray.direction.z * ray.direction.z;\n"
              "  if (abs(a) <= kernelEpsilon) {\n"
              "    return result;\n"
              "  }\n"
              "  const float b = 2.0f * (ray.origin.x * ray.direction.x +\n"
              "                          ray.origin.z * ray.direction.z);\n"
              "  const float c = ray.origin.x * ray.origin.x + ray.origin.z * ray.origin.z -\n"
              "                  radius * radius;\n"
              "  const float determinant = b * b - 4.0f * a * c;\n"
              "  if (determinant <= kernelEpsilon) {\n"
              "    return result;\n"
              "  }\n"
              "  const float determinantRoot = sqrt(determinant);\n"
              "  const float denominator = 2.0f * a;\n"
              "  const float distances[2] = {\n"
              "    (-determinantRoot - b) / denominator,\n"
              "    (determinantRoot - b) / denominator,\n"
              "  };\n"
              "  float bestDistance = positiveInfinity();\n"
              "  for (uint index = 0u; index != 2u; ++index) {\n"
              "    const float distance = distances[index];\n"
              "    if (distance <= 0.0f || distance < ray.minDistance ||\n"
              "        distance > ray.maxDistance || distance >= bestDistance) {\n"
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
              "  result.point = ray.origin + ray.direction * bestDistance;\n"
              "  result.point.w = 1.0f;\n"
              "  result.normal = float4(result.point.x * inverseRadius, 0.0f,\n"
              "                         result.point.z * inverseRadius, 0.0f);\n"
              "  const float twoPi = 6.28318530717958647692f;\n"
              "  float u = atan2(result.point.z, result.point.x) / twoPi;\n"
              "  if (u < 0.0f) {\n"
              "    u += 1.0f;\n"
              "  }\n"
              "  const float height = 2.0f * halfHeight;\n"
              "  const float v = height == 0.0f ? 0.0f : (result.point.y + halfHeight) / height;\n"
              "  result.uv = float4(u, v, 0.0f, 0.0f);\n"
              "  return result;\n"
              "}\n"
              "bool almostZero(float value) {\n"
              "  return abs(value) <= kernelEpsilon * 10.0f;\n"
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
              "    const float uTol = kernelEpsilon * 16.0f * (1.0f + abs(z * z) + abs(r));\n"
              "    const float vTol = kernelEpsilon * 16.0f * (1.0f + abs(2.0f * z) + abs(p));\n"
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
              "LocalHit intersectTorus(RayRecord ray, TorusPayload torus) {\n"
              "  LocalHit result = makeLocalMiss();\n"
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
              "  float bestDistance = positiveInfinity();\n"
              "  for (uint index = 0u; index != rootCount; ++index) {\n"
              "    const float distance = roots[index];\n"
              "    if (distance <= 0.0f || distance < ray.minDistance ||\n"
              "        distance > ray.maxDistance || distance >= bestDistance) {\n"
              "      continue;\n"
              "    }\n"
              "    bestDistance = distance;\n"
              "  }\n"
              "  if (!isfinite(bestDistance)) {\n"
              "    return result;\n"
              "  }\n"
              "  result.hit = true;\n"
              "  result.distance = bestDistance;\n"
              "  result.point = ray.origin + ray.direction * bestDistance;\n"
              "  result.point.w = 1.0f;\n"
              "  const float paramSquared = sweptRadius * sweptRadius + tubeRadius * tubeRadius;\n"
              "  const float sumSquared = dot(result.point.xyz, result.point.xyz);\n"
              "  result.normal = float4(normalize(float3(\n"
              "    4.0f * result.point.x * (sumSquared - paramSquared),\n"
              "    4.0f * result.point.y *\n"
              "      (sumSquared - paramSquared + 2.0f * sweptRadius * sweptRadius),\n"
              "    4.0f * result.point.z * (sumSquared - paramSquared))), 0.0f);\n"
              "  return result;\n"
              "}\n"
              "LocalHit intersectPrimitive(RayRecord ray, PrimitiveRecord primitive,\n"
              "                            device const TrianglePayload* triangles,\n"
              "                            device const SpherePayload* spheres,\n"
              "                            device const PlanePayload* planes,\n"
              "                            device const RectanglePayload* rectangles,\n"
              "                            device const DiskPayload* disks,\n"
              "                            device const OpenCylinderPayload* openCylinders,\n"
              "                            device const TorusPayload* tori,\n"
              "                            device const TransformPayload* transforms,\n"
              "                            uint triangleCount, uint sphereCount, uint planeCount,\n"
              "                            uint rectangleCount, uint diskCount,\n"
              "                            uint openCylinderCount, uint torusCount,\n"
              "                            uint transformCount) {\n"
              "  if (primitive.payloadCount == 0u) {\n"
              "    return makeLocalMiss();\n"
              "  }\n"
              "  RayRecord primitiveRay = ray;\n"
              "  bool hasTransform = primitive.transform != 0u;\n"
              "  TransformPayload transform = transforms[0];\n"
              "  if (hasTransform) {\n"
              "    if (primitive.transform >= transformCount) {\n"
              "      return makeLocalMiss();\n"
              "    }\n"
              "    transform = transforms[primitive.transform];\n"
              "    primitiveRay = transformRay(ray, transform);\n"
              "  }\n"
              "  LocalHit hit = makeLocalMiss();\n"
              "  if (primitive.kind == triangleKind && primitive.payloadOffset < triangleCount) {\n"
              "    hit = intersectTriangle(primitiveRay, triangles[primitive.payloadOffset]);\n"
              "  } else if (primitive.kind == sphereKind && primitive.payloadOffset < sphereCount) {\n"
              "    hit = intersectSphere(primitiveRay, spheres[primitive.payloadOffset]);\n"
              "  } else if (primitive.kind == planeKind && primitive.payloadOffset < planeCount) {\n"
              "    hit = intersectPlane(primitiveRay, planes[primitive.payloadOffset]);\n"
              "  } else if (primitive.kind == rectangleKind &&\n"
              "             primitive.payloadOffset < rectangleCount) {\n"
              "    hit = intersectRectangle(primitiveRay, rectangles[primitive.payloadOffset]);\n"
              "  } else if (primitive.kind == diskKind && primitive.payloadOffset < diskCount) {\n"
              "    hit = intersectDisk(primitiveRay, disks[primitive.payloadOffset]);\n"
              "  } else if (primitive.kind == openCylinderKind &&\n"
              "             primitive.payloadOffset < openCylinderCount) {\n"
              "    hit = intersectOpenCylinder(primitiveRay, openCylinders[primitive.payloadOffset]);\n"
              "  } else if (primitive.kind == torusKind && primitive.payloadOffset < torusCount) {\n"
              "    hit = intersectTorus(primitiveRay, tori[primitive.payloadOffset]);\n"
              "  }\n"
              "  if (hasTransform) {\n"
              "    return transformHit(hit, transform);\n"
              "  }\n"
              "  return hit;\n"
              "}\n"
              "bool hitOccludes(LocalHit hit, float maxDistance) {\n"
              "  if (!hit.hit) {\n"
              "    return false;\n"
              "  }\n"
              "  if (isinf(maxDistance)) {\n"
              "    return true;\n"
              "  }\n"
              "  const float occlusionLimit = max(0.0f, maxDistance - rayOcclusionEpsilon);\n"
              "  return hit.distance < occlusionLimit;\n"
              "}\n"
              "HitRecord makeMiss(RayRecord ray) {\n"
              "  HitRecord record;\n"
              "  record.hit = 0u;\n"
              "  record.material = 0u;\n"
              "  record.object = 0u;\n"
              "  record.primitiveRecord = 0u;\n"
              "  record.rayIndex = ray.rayIndex;\n"
              "  record.reservedId0 = 0u;\n"
              "  record.reservedId1 = 0u;\n"
              "  record.reservedId2 = 0u;\n"
              "  record.distance = positiveInfinity();\n"
              "  record.reservedDistance0 = 0.0f;\n"
              "  record.reservedDistance1 = 0.0f;\n"
              "  record.reservedDistance2 = 0.0f;\n"
              "  record.point = float4(0.0f);\n"
              "  record.normal = float4(0.0f);\n"
              "  record.uv = float4(0.0f);\n"
              "  record.barycentric = float4(0.0f);\n"
              "  return record;\n"
              "}\n"
              "OcclusionRecord makeOcclusion(RayRecord ray, bool occluded) {\n"
              "  OcclusionRecord record;\n"
              "  record.occluded = occluded ? 1u : 0u;\n"
              "  record.rayIndex = ray.rayIndex;\n"
              "  record.reserved0 = 0u;\n"
              "  record.reserved1 = 0u;\n"
              "  return record;\n"
              "}\n"
              "kernel void basicClosestHitKernel(device const BvhNode* bvh [[buffer(0)]],\n"
              "                                  device const PrimitiveRecord* primitives [[buffer(1)]],\n"
              "                                  device const TrianglePayload* triangles [[buffer(2)]],\n"
              "                                  device const SpherePayload* spheres [[buffer(3)]],\n"
              "                                  device const PlanePayload* planes [[buffer(4)]],\n"
              "                                  device const RectanglePayload* rectangles [[buffer(5)]],\n"
              "                                  device const DiskPayload* disks [[buffer(6)]],\n"
              "                                  device const OpenCylinderPayload* openCylinders [[buffer(7)]],\n"
              "                                  device const TorusPayload* tori [[buffer(8)]],\n"
              "                                  device const TransformPayload* transforms [[buffer(9)]],\n"
              "                                  device const RayRecord* rays [[buffer(10)]],\n"
              "                                  device HitRecord* hits [[buffer(11)]],\n"
              "                                  constant uint4& counts0 [[buffer(12)]],\n"
              "                                  constant uint4& counts1 [[buffer(13)]],\n"
              "                                  constant uint4& counts2 [[buffer(14)]],\n"
              "                                  uint id [[thread_position_in_grid]]) {\n"
              "  const uint bvhCount = counts0.x;\n"
              "  const uint primitiveCount = counts0.y;\n"
              "  const uint triangleCount = counts0.z;\n"
              "  const uint sphereCount = counts0.w;\n"
              "  const uint planeCount = counts1.x;\n"
              "  const uint rectangleCount = counts1.y;\n"
              "  const uint diskCount = counts1.z;\n"
              "  const uint openCylinderCount = counts1.w;\n"
              "  const uint rayCount = counts2.x;\n"
              "  const uint transformCount = counts2.y;\n"
              "  const uint torusCount = counts2.z;\n"
              "  if (id >= rayCount) {\n"
              "    return;\n"
              "  }\n"
              "  const RayRecord ray = rays[id];\n"
              "  HitRecord closest = makeMiss(ray);\n"
              "  if (bvhCount == 0u || primitiveCount == 0u) {\n"
              "    hits[id] = closest;\n"
              "    return;\n"
              "  }\n"
              "  uint stack[64];\n"
              "  uint stackSize = 1u;\n"
              "  stack[0] = 0u;\n"
              "  while (stackSize > 0u) {\n"
              "    const uint nodeIndex = stack[--stackSize];\n"
              "    if (nodeIndex >= bvhCount) {\n"
              "      continue;\n"
              "    }\n"
              "    const BvhNode node = bvh[nodeIndex];\n"
              "    if (!boundsIntersect(node.bounds, ray, closest.distance)) {\n"
              "      continue;\n"
              "    }\n"
              "    if ((node.flags & leafNodeFlag) == 0u) {\n"
              "      const uint leftChild = node.leftOrFirstPrimitive;\n"
              "      const uint rightChild = node.primitiveCount;\n"
              "      const float leftEntry = leftChild < bvhCount\n"
              "        ? boundsEntryDistance(bvh[leftChild].bounds, ray, closest.distance)\n"
              "        : positiveInfinity();\n"
              "      const float rightEntry = rightChild < bvhCount\n"
              "        ? boundsEntryDistance(bvh[rightChild].bounds, ray, closest.distance)\n"
              "        : positiveInfinity();\n"
              "      if (stackSize + 2u <= 64u) {\n"
              "        if (leftEntry < positiveInfinity() && rightEntry < positiveInfinity()) {\n"
              "          if (leftEntry <= rightEntry) {\n"
              "            stack[stackSize++] = rightChild;\n"
              "            stack[stackSize++] = leftChild;\n"
              "          } else {\n"
              "            stack[stackSize++] = leftChild;\n"
              "            stack[stackSize++] = rightChild;\n"
              "          }\n"
              "        } else if (leftEntry < positiveInfinity()) {\n"
              "          stack[stackSize++] = leftChild;\n"
              "        } else if (rightEntry < positiveInfinity()) {\n"
              "          stack[stackSize++] = rightChild;\n"
              "        }\n"
              "      }\n"
              "      continue;\n"
              "    }\n"
              "    for (uint offset = 0u; offset != node.primitiveCount; ++offset) {\n"
              "      const uint primitiveIndex = node.leftOrFirstPrimitive + offset;\n"
              "      if (primitiveIndex >= primitiveCount) {\n"
              "        continue;\n"
              "      }\n"
              "      const PrimitiveRecord primitive = primitives[primitiveIndex];\n"
              "      if (!boundsIntersect(primitive.bounds, ray, closest.distance)) {\n"
              "        continue;\n"
              "      }\n"
              "      const LocalHit hit = intersectPrimitive(\n"
              "        ray, primitive, triangles, spheres, planes, rectangles, disks,\n"
              "        openCylinders, tori, transforms,\n"
              "        triangleCount, sphereCount, planeCount, rectangleCount, diskCount,\n"
              "        openCylinderCount, torusCount, transformCount);\n"
              "      if (!hit.hit || (closest.hit != 0u && hit.distance >= closest.distance)) {\n"
              "        continue;\n"
              "      }\n"
              "      closest.hit = 1u;\n"
              "      closest.material = primitive.material;\n"
              "      closest.object = primitive.object;\n"
              "      closest.primitiveRecord = primitiveIndex;\n"
              "      closest.rayIndex = ray.rayIndex;\n"
              "      closest.distance = hit.distance;\n"
              "      closest.point = hit.point;\n"
              "      closest.normal = hit.normal;\n"
              "      closest.uv = hit.uv;\n"
              "      closest.barycentric = hit.barycentric;\n"
              "    }\n"
              "  }\n"
              "  hits[id] = closest;\n"
              "}\n"
              "kernel void basicAnyHitKernel(device const BvhNode* bvh [[buffer(0)]],\n"
              "                              device const PrimitiveRecord* primitives [[buffer(1)]],\n"
              "                              device const TrianglePayload* triangles [[buffer(2)]],\n"
              "                              device const SpherePayload* spheres [[buffer(3)]],\n"
              "                              device const PlanePayload* planes [[buffer(4)]],\n"
              "                              device const RectanglePayload* rectangles [[buffer(5)]],\n"
              "                              device const DiskPayload* disks [[buffer(6)]],\n"
              "                              device const OpenCylinderPayload* openCylinders [[buffer(7)]],\n"
              "                              device const TorusPayload* tori [[buffer(8)]],\n"
              "                              device const TransformPayload* transforms [[buffer(9)]],\n"
              "                              device const RayRecord* rays [[buffer(10)]],\n"
              "                              device OcclusionRecord* occlusion [[buffer(11)]],\n"
              "                              constant uint4& counts0 [[buffer(12)]],\n"
              "                              constant uint4& counts1 [[buffer(13)]],\n"
              "                              constant uint4& counts2 [[buffer(14)]],\n"
              "                              uint id [[thread_position_in_grid]]) {\n"
              "  const uint bvhCount = counts0.x;\n"
              "  const uint primitiveCount = counts0.y;\n"
              "  const uint triangleCount = counts0.z;\n"
              "  const uint sphereCount = counts0.w;\n"
              "  const uint planeCount = counts1.x;\n"
              "  const uint rectangleCount = counts1.y;\n"
              "  const uint diskCount = counts1.z;\n"
              "  const uint openCylinderCount = counts1.w;\n"
              "  const uint rayCount = counts2.x;\n"
              "  const uint transformCount = counts2.y;\n"
              "  const uint torusCount = counts2.z;\n"
              "  if (id >= rayCount) {\n"
              "    return;\n"
              "  }\n"
              "  const RayRecord ray = rays[id];\n"
              "  if (bvhCount == 0u || primitiveCount == 0u) {\n"
              "    occlusion[id] = makeOcclusion(ray, false);\n"
              "    return;\n"
              "  }\n"
              "  uint stack[64];\n"
              "  uint stackSize = 1u;\n"
              "  stack[0] = 0u;\n"
              "  while (stackSize > 0u) {\n"
              "    const uint nodeIndex = stack[--stackSize];\n"
              "    if (nodeIndex >= bvhCount) {\n"
              "      continue;\n"
              "    }\n"
              "    const BvhNode node = bvh[nodeIndex];\n"
              "    if (!boundsIntersect(node.bounds, ray, ray.maxDistance)) {\n"
              "      continue;\n"
              "    }\n"
              "    if ((node.flags & leafNodeFlag) == 0u) {\n"
              "      if (stackSize + 2u <= 64u) {\n"
              "        stack[stackSize++] = node.primitiveCount;\n"
              "        stack[stackSize++] = node.leftOrFirstPrimitive;\n"
              "      }\n"
              "      continue;\n"
              "    }\n"
              "    for (uint offset = 0u; offset != node.primitiveCount; ++offset) {\n"
              "      const uint primitiveIndex = node.leftOrFirstPrimitive + offset;\n"
              "      if (primitiveIndex >= primitiveCount) {\n"
              "        continue;\n"
              "      }\n"
              "      const PrimitiveRecord primitive = primitives[primitiveIndex];\n"
              "      if (!boundsIntersect(primitive.bounds, ray, ray.maxDistance)) {\n"
              "        continue;\n"
              "      }\n"
              "      const LocalHit hit = intersectPrimitive(\n"
              "        ray, primitive, triangles, spheres, planes, rectangles, disks,\n"
              "        openCylinders, tori, transforms,\n"
              "        triangleCount, sphereCount, planeCount, rectangleCount, diskCount,\n"
              "        openCylinderCount, torusCount, transformCount);\n"
              "      if (hitOccludes(hit, ray.maxDistance)) {\n"
              "        occlusion[id] = makeOcclusion(ray, true);\n"
              "        return;\n"
              "      }\n"
              "    }\n"
              "  }\n"
              "  occlusion[id] = makeOcclusion(ray, false);\n"
              "}\n";
    }

    std::runtime_error metalError(const char* context, NSError* error) {
      std::string message = context;
      if (error && error.localizedDescription) {
        message += ": ";
        message += error.localizedDescription.UTF8String;
      }
      return std::runtime_error(message);
    }

    id<MTLComputePipelineState> newPipeline(id<MTLDevice> device, NSString* source,
                                            NSString* functionName) {
      NSError* error = nil;
      id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
      if (!library) {
        throw metalError("Metal wavefront kernel library creation failed", error);
      }

      id<MTLFunction> function = [library newFunctionWithName:functionName];
      if (!function) {
        std::string message = "Metal wavefront kernel function was not found: ";
        message += functionName.UTF8String;
        throw std::runtime_error(message);
      }

      id<MTLComputePipelineState> pipeline =
        [device newComputePipelineStateWithFunction:function error:&error];
      if (!pipeline) {
        throw metalError("Metal wavefront kernel pipeline creation failed", error);
      }
      return pipeline;
    }

    id<MTLDevice> sharedMetalDevice() {
      static id<MTLDevice> device = MTLCreateSystemDefaultDevice();
      return device;
    }

    id<MTLCommandQueue> sharedCommandQueue() {
      id<MTLDevice> device = sharedMetalDevice();
      if (!device) {
        return nil;
      }

      static id<MTLCommandQueue> queue = [device newCommandQueue];
      return queue;
    }

    id<MTLComputePipelineState> sharedSmokePipeline() {
      id<MTLDevice> device = sharedMetalDevice();
      if (!device) {
        return nil;
      }

      static id<MTLComputePipelineState> pipeline =
        newPipeline(device, smokeKernelSource(), @"wavefrontSmokeKernel");
      return pipeline;
    }

    id<MTLComputePipelineState> sharedRayCompactionPipeline() {
      id<MTLDevice> device = sharedMetalDevice();
      if (!device) {
        return nil;
      }

      static id<MTLComputePipelineState> pipeline =
        newPipeline(device, rayCompactionKernelSource(), @"compactRayBatch");
      return pipeline;
    }

    id<MTLComputePipelineState> sharedBasicClosestHitPipeline() {
      id<MTLDevice> device = sharedMetalDevice();
      if (!device) {
        return nil;
      }

      static id<MTLComputePipelineState> pipeline =
        newPipeline(device, basicHitKernelSource(), @"basicClosestHitKernel");
      return pipeline;
    }

    id<MTLComputePipelineState> sharedBasicAnyHitPipeline() {
      id<MTLDevice> device = sharedMetalDevice();
      if (!device) {
        return nil;
      }

      static id<MTLComputePipelineState> pipeline =
        newPipeline(device, basicHitKernelSource(), @"basicAnyHitKernel");
      return pipeline;
    }

    template<typename T>
    id<MTLBuffer> newBufferFromVector(id<MTLDevice> device, const std::vector<T>& records) {
      if (records.empty()) {
        return nil;
      }
      return [device newBufferWithBytes:records.data()
                                 length:records.size() * sizeof(T)
                                options:MTLResourceStorageModeShared];
    }

    template<typename T>
    id<MTLBuffer> newPayloadBufferFromVector(id<MTLDevice> device,
                                             const std::vector<T>& records) {
      if (!records.empty()) {
        return newBufferFromVector(device, records);
      }

      const T emptyRecord{};
      return [device newBufferWithBytes:&emptyRecord
                                 length:sizeof(T)
                                options:MTLResourceStorageModeShared];
    }

    struct MetalBufferGuard {
      ~MetalBufferGuard() {
#if !__has_feature(objc_arc)
        [buffer release];
#endif
      }

      id<MTLBuffer> buffer{nil};
    };

    void validateRetainedRayIndices(const std::vector<std::uint32_t>& retainedRayIndices,
                                    std::uint64_t rayCount, const char* context) {
      if (retainedRayIndices.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error(std::string(context) + " retained ray count is too large");
      }
      for (const std::uint32_t index : retainedRayIndices) {
        if (index >= rayCount) {
          throw std::out_of_range(std::string(context) + " retained ray index is out of range");
        }
      }
    }

    void dispatchOneDimensional(id<MTLComputeCommandEncoder> encoder,
                                id<MTLComputePipelineState> pipeline, NSUInteger count) {
      const NSUInteger maxThreads =
        std::max<NSUInteger>(1, pipeline.maxTotalThreadsPerThreadgroup);
      const MTLSize gridSize = MTLSizeMake(count, 1, 1);
      const MTLSize threadgroupSize = MTLSizeMake(std::min<NSUInteger>(count, maxThreads), 1, 1);
      [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];
    }
  }

  bool MetalWavefrontSmokeKernel::deviceAvailable() const {
    @autoreleasepool {
      return sharedMetalDevice() != nil;
    }
  }

  std::string MetalWavefrontSmokeKernel::deviceUnavailableReason() const {
    @autoreleasepool {
      if (sharedMetalDevice()) {
        return "";
      }
      return "MTLCreateSystemDefaultDevice returned nil";
    }
  }

  bool MetalWavefrontSmokeKernel::renderPathAvailable() const {
    return renderPathUnavailableReason().empty();
  }

  std::string MetalWavefrontSmokeKernel::renderPathUnavailableReason() const {
    @autoreleasepool {
      if (!sharedMetalDevice()) {
        return deviceUnavailableReason();
      }
      if (!sharedCommandQueue()) {
        return "Metal default device did not create a command queue";
      }
      try {
        if (!sharedBasicClosestHitPipeline()) {
          return "Metal basic closest-hit pipeline was not created";
        }
        if (!sharedBasicAnyHitPipeline()) {
          return "Metal basic any-hit pipeline was not created";
        }
        if (!sharedRayCompactionPipeline()) {
          return "Metal ray-compaction pipeline was not created";
        }
        return "";
      } catch (const std::exception& e) {
        return e.what();
      }
    }
  }

  struct MetalWavefrontPreparedRayBatch::Private {
    id<MTLBuffer> rayBuffer{nil};
    id<MTLBuffer> counts2Buffer{nil};
    std::uint64_t rayCount{0};

    ~Private() {
#if !__has_feature(objc_arc)
      [rayBuffer release];
      [counts2Buffer release];
#endif
    }
  };

  MetalWavefrontPreparedRayBatch::MetalWavefrontPreparedRayBatch()
      : p(std::make_unique<Private>()) {
  }

  MetalWavefrontPreparedRayBatch::~MetalWavefrontPreparedRayBatch() = default;

  std::uint64_t MetalWavefrontPreparedRayBatch::rayCount() const {
    return p->rayCount;
  }

  std::uint64_t MetalWavefrontPreparedRayBatch::packedRayBytes() const {
    return p->rayCount * sizeof(GpuIntersectionRay);
  }

  struct MetalWavefrontPreparedScene::Private {
    id<MTLBuffer> bvhBuffer{nil};
    id<MTLBuffer> primitiveBuffer{nil};
    id<MTLBuffer> triangleBuffer{nil};
    id<MTLBuffer> sphereBuffer{nil};
    id<MTLBuffer> planeBuffer{nil};
    id<MTLBuffer> rectangleBuffer{nil};
    id<MTLBuffer> diskBuffer{nil};
    id<MTLBuffer> openCylinderBuffer{nil};
    id<MTLBuffer> torusBuffer{nil};
    id<MTLBuffer> transformBuffer{nil};
    id<MTLBuffer> counts0Buffer{nil};
    id<MTLBuffer> counts1Buffer{nil};
    std::uint32_t transformCount{0};
    std::uint32_t torusCount{0};
    mutable std::mutex queryBufferMutex;

    struct QueryBuffers {
      id<MTLBuffer> rayBuffer{nil};
      id<MTLBuffer> closestHitBuffer{nil};
      id<MTLBuffer> anyHitBuffer{nil};
      id<MTLBuffer> counts2Buffer{nil};
      NSUInteger rayCapacity{0};
      NSUInteger closestHitCapacity{0};
      NSUInteger anyHitCapacity{0};
      bool inUse{false};

      ~QueryBuffers() {
#if !__has_feature(objc_arc)
        [rayBuffer release];
        [closestHitBuffer release];
        [anyHitBuffer release];
        [counts2Buffer release];
#endif
      }
    };

    struct QueryBufferLease {
      QueryBufferLease(const Private& owner, QueryBuffers& buffers)
          : owner(&owner), buffers(&buffers) {
      }

      QueryBufferLease(const QueryBufferLease&) = delete;
      QueryBufferLease& operator=(const QueryBufferLease&) = delete;

      QueryBufferLease(QueryBufferLease&& other) noexcept
          : owner(other.owner), buffers(other.buffers) {
        other.owner = nullptr;
        other.buffers = nullptr;
      }

      ~QueryBufferLease() {
        if (owner && buffers) {
          owner->releaseQueryBuffers(*buffers);
        }
      }

      QueryBuffers& get() const {
        return *buffers;
      }

      const Private* owner;
      QueryBuffers* buffers;
    };

    mutable std::vector<std::unique_ptr<QueryBuffers>> queryBufferPool;

    ~Private() {
#if !__has_feature(objc_arc)
      [bvhBuffer release];
      [primitiveBuffer release];
      [triangleBuffer release];
      [sphereBuffer release];
      [planeBuffer release];
      [rectangleBuffer release];
      [diskBuffer release];
      [openCylinderBuffer release];
      [torusBuffer release];
      [transformBuffer release];
      [counts0Buffer release];
      [counts1Buffer release];
#endif
    }

    QueryBufferLease acquireQueryBuffers() const {
      std::lock_guard<std::mutex> lock(queryBufferMutex);
      for (const auto& buffers : queryBufferPool) {
        if (!buffers->inUse) {
          buffers->inUse = true;
          return QueryBufferLease(*this, *buffers);
        }
      }

      auto buffers = std::make_unique<QueryBuffers>();
      buffers->inUse = true;
      queryBufferPool.push_back(std::move(buffers));
      return QueryBufferLease(*this, *queryBufferPool.back());
    }

    void releaseQueryBuffers(QueryBuffers& buffers) const {
      std::lock_guard<std::mutex> lock(queryBufferMutex);
      buffers.inUse = false;
    }

    template<typename T>
    id<MTLBuffer> reusableBuffer(id<MTLDevice> device, __strong id<MTLBuffer>& buffer,
                                 NSUInteger& capacity, std::size_t count) const {
      if (count > std::numeric_limits<NSUInteger>::max() / sizeof(T)) {
        throw std::runtime_error("Metal prepared wavefront query buffer is too large");
      }

      if (buffer && capacity >= count) {
        return buffer;
      }

#if !__has_feature(objc_arc)
      [buffer release];
#endif
      capacity = static_cast<NSUInteger>(count);
      buffer = [device newBufferWithLength:capacity * sizeof(T)
                                   options:MTLResourceStorageModeShared];
      return buffer;
    }

    id<MTLBuffer> uploadRays(QueryBuffers& buffers, id<MTLDevice> device,
                             const std::vector<GpuIntersectionRay>& rays) const {
      id<MTLBuffer> buffer =
        reusableBuffer<GpuIntersectionRay>(device, buffers.rayBuffer, buffers.rayCapacity,
                                           rays.size());
      if (!buffer) {
        return nil;
      }
      std::memcpy(buffer.contents, rays.data(), rays.size() * sizeof(rays.front()));
      return buffer;
    }

    id<MTLBuffer> closestHits(QueryBuffers& buffers, id<MTLDevice> device,
                              std::size_t count) const {
      return reusableBuffer<GpuIntersectionHitRecord>(
        device, buffers.closestHitBuffer, buffers.closestHitCapacity, count);
    }

    id<MTLBuffer> anyHits(QueryBuffers& buffers, id<MTLDevice> device, std::size_t count) const {
      return reusableBuffer<GpuIntersectionOcclusionRecord>(
        device, buffers.anyHitBuffer, buffers.anyHitCapacity, count);
    }

    id<MTLBuffer> counts2(QueryBuffers& buffers, id<MTLDevice> device,
                          std::size_t rayCount) const {
      if (rayCount > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("Metal prepared wavefront query has too many rays");
      }

      if (!buffers.counts2Buffer) {
        buffers.counts2Buffer = [device newBufferWithLength:4 * sizeof(std::uint32_t)
                                                    options:MTLResourceStorageModeShared];
      }
      if (!buffers.counts2Buffer) {
        return nil;
      }

      const std::array<std::uint32_t, 4> counts{
        static_cast<std::uint32_t>(rayCount),
        transformCount,
        torusCount,
        0u,
      };
      std::memcpy(buffers.counts2Buffer.contents, counts.data(),
                  counts.size() * sizeof(std::uint32_t));
      return buffers.counts2Buffer;
    }

    void setSceneBuffers(id<MTLComputeCommandEncoder> encoder) const {
      [encoder setBuffer:bvhBuffer offset:0 atIndex:0];
      [encoder setBuffer:primitiveBuffer offset:0 atIndex:1];
      [encoder setBuffer:triangleBuffer offset:0 atIndex:2];
      [encoder setBuffer:sphereBuffer offset:0 atIndex:3];
      [encoder setBuffer:planeBuffer offset:0 atIndex:4];
      [encoder setBuffer:rectangleBuffer offset:0 atIndex:5];
      [encoder setBuffer:diskBuffer offset:0 atIndex:6];
      [encoder setBuffer:openCylinderBuffer offset:0 atIndex:7];
      [encoder setBuffer:torusBuffer offset:0 atIndex:8];
      [encoder setBuffer:transformBuffer offset:0 atIndex:9];
    }

    void setCountBuffers(id<MTLComputeCommandEncoder> encoder, id<MTLBuffer> counts2Buffer) const {
      [encoder setBuffer:counts0Buffer offset:0 atIndex:12];
      [encoder setBuffer:counts1Buffer offset:0 atIndex:13];
      [encoder setBuffer:counts2Buffer offset:0 atIndex:14];
    }
  };

  MetalWavefrontPreparedScene::MetalWavefrontPreparedScene(
    const GpuIntersectionSceneBuffers& scene)
      : p(std::make_unique<Private>()) {
    if (!scene.basicHitKernelEligible()) {
      throw std::invalid_argument(
        "Metal prepared wavefront scene requires a supported primitive scene");
    }

    @autoreleasepool {
      id<MTLDevice> device = sharedMetalDevice();
      if (!device) {
        throw std::runtime_error("Metal prepared wavefront scene requires a Metal device");
      }
      if (!MetalWavefrontSmokeKernel().renderPathAvailable()) {
        throw std::runtime_error(
          "Metal prepared wavefront scene requires the Metal basic hit kernels");
      }

      p->bvhBuffer = newBufferFromVector(device, scene.bvh);
      p->primitiveBuffer = newBufferFromVector(device, scene.primitives);
      p->triangleBuffer = newPayloadBufferFromVector(device, scene.triangles);
      p->sphereBuffer = newPayloadBufferFromVector(device, scene.spheres);
      p->planeBuffer = newPayloadBufferFromVector(device, scene.planes);
      p->rectangleBuffer = newPayloadBufferFromVector(device, scene.rectangles);
      p->diskBuffer = newPayloadBufferFromVector(device, scene.disks);
      p->openCylinderBuffer = newPayloadBufferFromVector(device, scene.openCylinders);
      p->torusBuffer = newPayloadBufferFromVector(device, scene.tori);
      p->transformBuffer = newPayloadBufferFromVector(device, scene.transforms);
      p->transformCount = static_cast<std::uint32_t>(scene.transforms.size());
      p->torusCount = static_cast<std::uint32_t>(scene.tori.size());

      const std::array<std::uint32_t, 4> counts0{
        static_cast<std::uint32_t>(scene.bvh.size()),
        static_cast<std::uint32_t>(scene.primitives.size()),
        static_cast<std::uint32_t>(scene.triangles.size()),
        static_cast<std::uint32_t>(scene.spheres.size()),
      };
      const std::array<std::uint32_t, 4> counts1{
        static_cast<std::uint32_t>(scene.planes.size()),
        static_cast<std::uint32_t>(scene.rectangles.size()),
        static_cast<std::uint32_t>(scene.disks.size()),
        static_cast<std::uint32_t>(scene.openCylinders.size()),
      };
      p->counts0Buffer =
        [device newBufferWithBytes:counts0.data()
                             length:counts0.size() * sizeof(std::uint32_t)
                            options:MTLResourceStorageModeShared];
      p->counts1Buffer =
        [device newBufferWithBytes:counts1.data()
                             length:counts1.size() * sizeof(std::uint32_t)
                            options:MTLResourceStorageModeShared];

      if (!p->bvhBuffer || !p->primitiveBuffer || !p->triangleBuffer || !p->sphereBuffer ||
          !p->planeBuffer || !p->rectangleBuffer || !p->diskBuffer ||
          !p->openCylinderBuffer || !p->torusBuffer || !p->transformBuffer || !p->counts0Buffer ||
          !p->counts1Buffer) {
        throw std::runtime_error("Metal prepared wavefront scene buffer allocation failed");
      }
    }
  }

  MetalWavefrontPreparedScene::~MetalWavefrontPreparedScene() = default;

  std::shared_ptr<const MetalWavefrontPreparedRayBatch>
  MetalWavefrontPreparedScene::prepareRays(const std::vector<GpuIntersectionRay>& rays) const {
    auto batch = std::shared_ptr<MetalWavefrontPreparedRayBatch>(new MetalWavefrontPreparedRayBatch);
    if (rays.empty()) {
      return batch;
    }
    if (rays.size() > std::numeric_limits<std::uint32_t>::max()) {
      throw std::runtime_error("Metal prepared wavefront ray batch has too many rays");
    }

    @autoreleasepool {
      id<MTLDevice> device = sharedMetalDevice();
      if (!device) {
        throw std::runtime_error("Metal prepared ray batch requires a Metal device");
      }

      batch->p->rayCount = static_cast<std::uint64_t>(rays.size());
      batch->p->rayBuffer = newBufferFromVector(device, rays);
      const std::array<std::uint32_t, 4> counts{
        static_cast<std::uint32_t>(rays.size()),
        p->transformCount,
        p->torusCount,
        0u,
      };
      batch->p->counts2Buffer =
        [device newBufferWithBytes:counts.data()
                             length:counts.size() * sizeof(std::uint32_t)
                            options:MTLResourceStorageModeShared];

      if (!batch->p->rayBuffer || !batch->p->counts2Buffer) {
        throw std::runtime_error("Metal prepared ray batch buffer allocation failed");
      }
    }
    return batch;
  }

  std::shared_ptr<const MetalWavefrontPreparedRayBatch> MetalWavefrontPreparedScene::compactRays(
    const MetalWavefrontPreparedRayBatch& sourceRays,
    const std::vector<std::uint32_t>& retainedRayIndices) const {
    return compactRaysTimed(sourceRays, retainedRayIndices).rays;
  }

  MetalWavefrontRayBatchCompactionResult MetalWavefrontPreparedScene::compactRaysTimed(
    const MetalWavefrontPreparedRayBatch& sourceRays,
    const std::vector<std::uint32_t>& retainedRayIndices) const {
    MetalWavefrontRayBatchCompactionResult result;
    auto batch = std::shared_ptr<MetalWavefrontPreparedRayBatch>(new MetalWavefrontPreparedRayBatch);
    result.rays = batch;
    if (retainedRayIndices.empty()) {
      return result;
    }
    validateRetainedRayIndices(retainedRayIndices, sourceRays.rayCount(),
                               "Metal prepared ray-batch compaction");

    @autoreleasepool {
      const auto uploadStart = std::chrono::steady_clock::now();
      id<MTLDevice> device = sharedMetalDevice();
      if (!device) {
        throw std::runtime_error("Metal prepared ray-batch compaction requires a Metal device");
      }
      if (!sourceRays.p->rayBuffer) {
        throw std::runtime_error("Metal prepared ray-batch compaction requires source rays");
      }

      id<MTLComputePipelineState> pipeline = sharedRayCompactionPipeline();
      MetalBufferGuard retainedIndexBuffer;
      retainedIndexBuffer.buffer =
        [device newBufferWithBytes:retainedRayIndices.data()
                             length:retainedRayIndices.size() * sizeof(std::uint32_t)
                            options:MTLResourceStorageModeShared];
      batch->p->rayCount = static_cast<std::uint64_t>(retainedRayIndices.size());
      batch->p->rayBuffer =
        [device newBufferWithLength:retainedRayIndices.size() * sizeof(GpuIntersectionRay)
                            options:MTLResourceStorageModeShared];
      const std::array<std::uint32_t, 4> counts{
        static_cast<std::uint32_t>(retainedRayIndices.size()),
        p->transformCount,
        p->torusCount,
        0u,
      };
      batch->p->counts2Buffer =
        [device newBufferWithBytes:counts.data()
                             length:counts.size() * sizeof(std::uint32_t)
                            options:MTLResourceStorageModeShared];

      if (!pipeline || !retainedIndexBuffer.buffer || !batch->p->rayBuffer ||
          !batch->p->counts2Buffer) {
        throw std::runtime_error("Metal prepared ray-batch compaction buffer allocation failed");
      }

      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!queue || !commandBuffer || !encoder) {
        throw std::runtime_error("Metal prepared ray-batch compaction command setup failed");
      }

      [encoder setComputePipelineState:pipeline];
      [encoder setBuffer:sourceRays.p->rayBuffer offset:0 atIndex:0];
      [encoder setBuffer:retainedIndexBuffer.buffer offset:0 atIndex:1];
      [encoder setBuffer:batch->p->rayBuffer offset:0 atIndex:2];
      dispatchOneDimensional(encoder, pipeline, retainedRayIndices.size());
      [encoder endEncoding];

      const auto uploadEnd = std::chrono::steady_clock::now();
      const auto kernelStart = uploadEnd;
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      const auto kernelEnd = std::chrono::steady_clock::now();

      if (commandBuffer.error) {
        throw metalError("Metal prepared ray-batch compaction dispatch failed",
                         commandBuffer.error);
      }
      result.timing.uploadSeconds = secondsBetween(uploadStart, uploadEnd);
      result.timing.kernelSeconds = secondsBetween(kernelStart, kernelEnd);
    }
    return result;
  }

  MetalWavefrontClosestHitKernelResult
  MetalWavefrontPreparedScene::runTimedBasicClosestHitKernel(
    const std::vector<GpuIntersectionRay>& rays) const {
    if (rays.empty()) {
      return {};
    }

    @autoreleasepool {
      MetalWavefrontClosestHitKernelResult result;
      const Private::QueryBufferLease queryBuffers = p->acquireQueryBuffers();
      const auto uploadStart = std::chrono::steady_clock::now();
      id<MTLDevice> device = sharedMetalDevice();
      if (!device) {
        throw std::runtime_error("Metal prepared closest-hit kernel requires a Metal device");
      }

      id<MTLComputePipelineState> pipeline = sharedBasicClosestHitPipeline();
      id<MTLBuffer> rayBuffer = p->uploadRays(queryBuffers.get(), device, rays);
      id<MTLBuffer> hitBuffer = p->closestHits(queryBuffers.get(), device, rays.size());
      id<MTLBuffer> counts2Buffer = p->counts2(queryBuffers.get(), device, rays.size());
      if (!rayBuffer || !hitBuffer || !counts2Buffer) {
        throw std::runtime_error("Metal prepared closest-hit query buffer allocation failed");
      }

      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!queue || !commandBuffer || !encoder) {
        throw std::runtime_error("Metal prepared closest-hit command setup failed");
      }
      const auto uploadEnd = std::chrono::steady_clock::now();

      [encoder setComputePipelineState:pipeline];
      p->setSceneBuffers(encoder);
      [encoder setBuffer:rayBuffer offset:0 atIndex:10];
      [encoder setBuffer:hitBuffer offset:0 atIndex:11];
      p->setCountBuffers(encoder, counts2Buffer);
      dispatchOneDimensional(encoder, pipeline, rays.size());
      [encoder endEncoding];
      const auto kernelStart = std::chrono::steady_clock::now();
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      const auto kernelEnd = std::chrono::steady_clock::now();

      if (commandBuffer.error) {
        throw metalError("Metal prepared closest-hit kernel dispatch failed", commandBuffer.error);
      }

      const auto readbackStart = std::chrono::steady_clock::now();
      result.hits.resize(rays.size());
      std::memcpy(result.hits.data(), hitBuffer.contents,
                  result.hits.size() * sizeof(result.hits.front()));
      const auto readbackEnd = std::chrono::steady_clock::now();
      result.timing.uploadSeconds = secondsBetween(uploadStart, uploadEnd);
      result.timing.kernelSeconds = secondsBetween(kernelStart, kernelEnd);
      result.timing.readbackSeconds = secondsBetween(readbackStart, readbackEnd);
      return result;
    }
  }

  MetalWavefrontClosestHitKernelResult MetalWavefrontPreparedScene::runTimedBasicClosestHitKernel(
    const MetalWavefrontPreparedRayBatch& rays) const {
    if (rays.rayCount() == 0) {
      return {};
    }

    @autoreleasepool {
      MetalWavefrontClosestHitKernelResult result;
      const Private::QueryBufferLease queryBuffers = p->acquireQueryBuffers();
      const auto uploadStart = std::chrono::steady_clock::now();
      id<MTLDevice> device = sharedMetalDevice();
      if (!device) {
        throw std::runtime_error("Metal prepared closest-hit kernel requires a Metal device");
      }

      id<MTLComputePipelineState> pipeline = sharedBasicClosestHitPipeline();
      id<MTLBuffer> hitBuffer = p->closestHits(queryBuffers.get(), device, rays.rayCount());
      if (!rays.p->rayBuffer || !rays.p->counts2Buffer || !hitBuffer) {
        throw std::runtime_error("Metal prepared closest-hit query buffer allocation failed");
      }

      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!queue || !commandBuffer || !encoder) {
        throw std::runtime_error("Metal prepared closest-hit command setup failed");
      }
      const auto uploadEnd = std::chrono::steady_clock::now();

      [encoder setComputePipelineState:pipeline];
      p->setSceneBuffers(encoder);
      [encoder setBuffer:rays.p->rayBuffer offset:0 atIndex:10];
      [encoder setBuffer:hitBuffer offset:0 atIndex:11];
      p->setCountBuffers(encoder, rays.p->counts2Buffer);
      dispatchOneDimensional(encoder, pipeline, rays.rayCount());
      [encoder endEncoding];
      const auto kernelStart = std::chrono::steady_clock::now();
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      const auto kernelEnd = std::chrono::steady_clock::now();

      if (commandBuffer.error) {
        throw metalError("Metal prepared closest-hit kernel dispatch failed", commandBuffer.error);
      }

      const auto readbackStart = std::chrono::steady_clock::now();
      result.hits.resize(static_cast<std::size_t>(rays.rayCount()));
      std::memcpy(result.hits.data(), hitBuffer.contents,
                  result.hits.size() * sizeof(result.hits.front()));
      const auto readbackEnd = std::chrono::steady_clock::now();
      result.timing.uploadSeconds = secondsBetween(uploadStart, uploadEnd);
      result.timing.kernelSeconds = secondsBetween(kernelStart, kernelEnd);
      result.timing.readbackSeconds = secondsBetween(readbackStart, readbackEnd);
      return result;
    }
  }

  MetalWavefrontAnyHitKernelResult MetalWavefrontPreparedScene::runTimedBasicAnyHitKernel(
    const std::vector<GpuIntersectionRay>& rays) const {
    if (rays.empty()) {
      return {};
    }

    @autoreleasepool {
      MetalWavefrontAnyHitKernelResult result;
      const Private::QueryBufferLease queryBuffers = p->acquireQueryBuffers();
      const auto uploadStart = std::chrono::steady_clock::now();
      id<MTLDevice> device = sharedMetalDevice();
      if (!device) {
        throw std::runtime_error("Metal prepared any-hit kernel requires a Metal device");
      }

      id<MTLComputePipelineState> pipeline = sharedBasicAnyHitPipeline();
      id<MTLBuffer> rayBuffer = p->uploadRays(queryBuffers.get(), device, rays);
      id<MTLBuffer> occlusionBuffer = p->anyHits(queryBuffers.get(), device, rays.size());
      id<MTLBuffer> counts2Buffer = p->counts2(queryBuffers.get(), device, rays.size());
      if (!rayBuffer || !occlusionBuffer || !counts2Buffer) {
        throw std::runtime_error("Metal prepared any-hit query buffer allocation failed");
      }

      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!queue || !commandBuffer || !encoder) {
        throw std::runtime_error("Metal prepared any-hit command setup failed");
      }
      const auto uploadEnd = std::chrono::steady_clock::now();

      [encoder setComputePipelineState:pipeline];
      p->setSceneBuffers(encoder);
      [encoder setBuffer:rayBuffer offset:0 atIndex:10];
      [encoder setBuffer:occlusionBuffer offset:0 atIndex:11];
      p->setCountBuffers(encoder, counts2Buffer);
      dispatchOneDimensional(encoder, pipeline, rays.size());
      [encoder endEncoding];
      const auto kernelStart = std::chrono::steady_clock::now();
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      const auto kernelEnd = std::chrono::steady_clock::now();

      if (commandBuffer.error) {
        throw metalError("Metal prepared any-hit kernel dispatch failed", commandBuffer.error);
      }

      const auto readbackStart = std::chrono::steady_clock::now();
      result.records.resize(rays.size());
      std::memcpy(result.records.data(), occlusionBuffer.contents,
                  result.records.size() * sizeof(result.records.front()));
      const auto readbackEnd = std::chrono::steady_clock::now();
      result.timing.uploadSeconds = secondsBetween(uploadStart, uploadEnd);
      result.timing.kernelSeconds = secondsBetween(kernelStart, kernelEnd);
      result.timing.readbackSeconds = secondsBetween(readbackStart, readbackEnd);
      return result;
    }
  }

  MetalWavefrontAnyHitKernelResult MetalWavefrontPreparedScene::runTimedBasicAnyHitKernel(
    const MetalWavefrontPreparedRayBatch& rays) const {
    if (rays.rayCount() == 0) {
      return {};
    }

    @autoreleasepool {
      MetalWavefrontAnyHitKernelResult result;
      const Private::QueryBufferLease queryBuffers = p->acquireQueryBuffers();
      const auto uploadStart = std::chrono::steady_clock::now();
      id<MTLDevice> device = sharedMetalDevice();
      if (!device) {
        throw std::runtime_error("Metal prepared any-hit kernel requires a Metal device");
      }

      id<MTLComputePipelineState> pipeline = sharedBasicAnyHitPipeline();
      id<MTLBuffer> occlusionBuffer = p->anyHits(queryBuffers.get(), device, rays.rayCount());
      if (!rays.p->rayBuffer || !rays.p->counts2Buffer || !occlusionBuffer) {
        throw std::runtime_error("Metal prepared any-hit query buffer allocation failed");
      }

      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!queue || !commandBuffer || !encoder) {
        throw std::runtime_error("Metal prepared any-hit command setup failed");
      }
      const auto uploadEnd = std::chrono::steady_clock::now();

      [encoder setComputePipelineState:pipeline];
      p->setSceneBuffers(encoder);
      [encoder setBuffer:rays.p->rayBuffer offset:0 atIndex:10];
      [encoder setBuffer:occlusionBuffer offset:0 atIndex:11];
      p->setCountBuffers(encoder, rays.p->counts2Buffer);
      dispatchOneDimensional(encoder, pipeline, rays.rayCount());
      [encoder endEncoding];
      const auto kernelStart = std::chrono::steady_clock::now();
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      const auto kernelEnd = std::chrono::steady_clock::now();

      if (commandBuffer.error) {
        throw metalError("Metal prepared any-hit kernel dispatch failed", commandBuffer.error);
      }

      const auto readbackStart = std::chrono::steady_clock::now();
      result.records.resize(static_cast<std::size_t>(rays.rayCount()));
      std::memcpy(result.records.data(), occlusionBuffer.contents,
                  result.records.size() * sizeof(result.records.front()));
      const auto readbackEnd = std::chrono::steady_clock::now();
      result.timing.uploadSeconds = secondsBetween(uploadStart, uploadEnd);
      result.timing.kernelSeconds = secondsBetween(kernelStart, kernelEnd);
      result.timing.readbackSeconds = secondsBetween(readbackStart, readbackEnd);
      return result;
    }
  }

  std::vector<std::uint32_t> MetalWavefrontSmokeKernel::runDummyHitMissKernel(
    const std::vector<std::uint32_t>& rayIds) const {
    if (rayIds.empty()) {
      return {};
    }

    @autoreleasepool {
      id<MTLDevice> device = sharedMetalDevice();
      if (!device) {
        throw std::runtime_error("Metal wavefront smoke kernel requires a Metal device");
      }

      id<MTLComputePipelineState> pipeline = sharedSmokePipeline();

      const NSUInteger byteCount = rayIds.size() * sizeof(std::uint32_t);
      id<MTLBuffer> inputBuffer = [device newBufferWithBytes:rayIds.data()
                                                      length:byteCount
                                                     options:MTLResourceStorageModeShared];
      id<MTLBuffer> outputBuffer = [device newBufferWithLength:byteCount
                                                       options:MTLResourceStorageModeShared];
      if (!inputBuffer || !outputBuffer) {
        throw std::runtime_error("Metal wavefront smoke kernel buffer allocation failed");
      }

      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!queue || !commandBuffer || !encoder) {
        throw std::runtime_error("Metal wavefront smoke kernel command setup failed");
      }

      [encoder setComputePipelineState:pipeline];
      [encoder setBuffer:inputBuffer offset:0 atIndex:0];
      [encoder setBuffer:outputBuffer offset:0 atIndex:1];

      dispatchOneDimensional(encoder, pipeline, rayIds.size());
      [encoder endEncoding];
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];

      if (commandBuffer.error) {
        throw metalError("Metal wavefront smoke kernel dispatch failed", commandBuffer.error);
      }

      std::vector<std::uint32_t> results(rayIds.size());
      std::memcpy(results.data(), outputBuffer.contents, byteCount);
      return results;
    }
  }

  std::vector<GpuIntersectionHitRecord>
  MetalWavefrontSmokeKernel::runBasicClosestHitKernel(
    const GpuIntersectionSceneBuffers& scene, const std::vector<GpuIntersectionRay>& rays) const {
    return runTimedBasicClosestHitKernel(scene, rays).hits;
  }

  MetalWavefrontClosestHitKernelResult
  MetalWavefrontSmokeKernel::runTimedBasicClosestHitKernel(
    const GpuIntersectionSceneBuffers& scene, const std::vector<GpuIntersectionRay>& rays) const {
    if (rays.empty()) {
      return {};
    }
    if (!scene.basicHitKernelEligible()) {
      throw std::invalid_argument(
        "Metal basic closest-hit kernel requires a supported primitive scene");
    }

    @autoreleasepool {
      MetalWavefrontClosestHitKernelResult result;
      const auto uploadStart = std::chrono::steady_clock::now();
      id<MTLDevice> device = sharedMetalDevice();
      if (!device) {
        throw std::runtime_error("Metal wavefront basic hit kernel requires a Metal device");
      }

      id<MTLComputePipelineState> pipeline = sharedBasicClosestHitPipeline();

      id<MTLBuffer> bvhBuffer = newBufferFromVector(device, scene.bvh);
      id<MTLBuffer> primitiveBuffer = newBufferFromVector(device, scene.primitives);
      id<MTLBuffer> triangleBuffer = newPayloadBufferFromVector(device, scene.triangles);
      id<MTLBuffer> sphereBuffer = newPayloadBufferFromVector(device, scene.spheres);
      id<MTLBuffer> planeBuffer = newPayloadBufferFromVector(device, scene.planes);
      id<MTLBuffer> rectangleBuffer = newPayloadBufferFromVector(device, scene.rectangles);
      id<MTLBuffer> diskBuffer = newPayloadBufferFromVector(device, scene.disks);
      id<MTLBuffer> openCylinderBuffer = newPayloadBufferFromVector(device, scene.openCylinders);
      id<MTLBuffer> torusBuffer = newPayloadBufferFromVector(device, scene.tori);
      id<MTLBuffer> transformBuffer = newPayloadBufferFromVector(device, scene.transforms);
      id<MTLBuffer> rayBuffer = newBufferFromVector(device, rays);
      const std::vector<GpuIntersectionHitRecord> initialHits(rays.size());
      id<MTLBuffer> hitBuffer = newBufferFromVector(device, initialHits);
      const std::array<std::uint32_t, 4> counts0{
        static_cast<std::uint32_t>(scene.bvh.size()),
        static_cast<std::uint32_t>(scene.primitives.size()),
        static_cast<std::uint32_t>(scene.triangles.size()),
        static_cast<std::uint32_t>(scene.spheres.size()),
      };
      const std::array<std::uint32_t, 4> counts1{
        static_cast<std::uint32_t>(scene.planes.size()),
        static_cast<std::uint32_t>(scene.rectangles.size()),
        static_cast<std::uint32_t>(scene.disks.size()),
        static_cast<std::uint32_t>(scene.openCylinders.size()),
      };
      const std::array<std::uint32_t, 4> counts2{
        static_cast<std::uint32_t>(rays.size()),
        static_cast<std::uint32_t>(scene.transforms.size()),
        static_cast<std::uint32_t>(scene.tori.size()),
        0u,
      };
      id<MTLBuffer> counts0Buffer =
        [device newBufferWithBytes:counts0.data()
                             length:counts0.size() * sizeof(std::uint32_t)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> counts1Buffer =
        [device newBufferWithBytes:counts1.data()
                             length:counts1.size() * sizeof(std::uint32_t)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> counts2Buffer =
        [device newBufferWithBytes:counts2.data()
                             length:counts2.size() * sizeof(std::uint32_t)
                            options:MTLResourceStorageModeShared];
      if (!bvhBuffer || !primitiveBuffer || !triangleBuffer || !sphereBuffer || !planeBuffer ||
          !rectangleBuffer || !diskBuffer || !openCylinderBuffer || !torusBuffer ||
          !transformBuffer ||
          !rayBuffer || !hitBuffer || !counts0Buffer || !counts1Buffer || !counts2Buffer) {
        throw std::runtime_error("Metal wavefront basic hit kernel buffer allocation failed");
      }

      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!queue || !commandBuffer || !encoder) {
        throw std::runtime_error("Metal wavefront basic hit kernel command setup failed");
      }
      const auto uploadEnd = std::chrono::steady_clock::now();

      [encoder setComputePipelineState:pipeline];
      [encoder setBuffer:bvhBuffer offset:0 atIndex:0];
      [encoder setBuffer:primitiveBuffer offset:0 atIndex:1];
      [encoder setBuffer:triangleBuffer offset:0 atIndex:2];
      [encoder setBuffer:sphereBuffer offset:0 atIndex:3];
      [encoder setBuffer:planeBuffer offset:0 atIndex:4];
      [encoder setBuffer:rectangleBuffer offset:0 atIndex:5];
      [encoder setBuffer:diskBuffer offset:0 atIndex:6];
      [encoder setBuffer:openCylinderBuffer offset:0 atIndex:7];
      [encoder setBuffer:torusBuffer offset:0 atIndex:8];
      [encoder setBuffer:transformBuffer offset:0 atIndex:9];
      [encoder setBuffer:rayBuffer offset:0 atIndex:10];
      [encoder setBuffer:hitBuffer offset:0 atIndex:11];
      [encoder setBuffer:counts0Buffer offset:0 atIndex:12];
      [encoder setBuffer:counts1Buffer offset:0 atIndex:13];
      [encoder setBuffer:counts2Buffer offset:0 atIndex:14];
      dispatchOneDimensional(encoder, pipeline, rays.size());
      [encoder endEncoding];
      const auto kernelStart = std::chrono::steady_clock::now();
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      const auto kernelEnd = std::chrono::steady_clock::now();

      if (commandBuffer.error) {
        throw metalError("Metal wavefront basic hit kernel dispatch failed", commandBuffer.error);
      }

      const auto readbackStart = std::chrono::steady_clock::now();
      result.hits.resize(rays.size());
      std::memcpy(result.hits.data(), hitBuffer.contents,
                  result.hits.size() * sizeof(result.hits.front()));
      const auto readbackEnd = std::chrono::steady_clock::now();
      result.timing.uploadSeconds = secondsBetween(uploadStart, uploadEnd);
      result.timing.kernelSeconds = secondsBetween(kernelStart, kernelEnd);
      result.timing.readbackSeconds = secondsBetween(readbackStart, readbackEnd);
      return result;
    }
  }

  std::vector<GpuIntersectionOcclusionRecord>
  MetalWavefrontSmokeKernel::runBasicAnyHitKernel(
    const GpuIntersectionSceneBuffers& scene, const std::vector<GpuIntersectionRay>& rays) const {
    return runTimedBasicAnyHitKernel(scene, rays).records;
  }

  MetalWavefrontAnyHitKernelResult
  MetalWavefrontSmokeKernel::runTimedBasicAnyHitKernel(
    const GpuIntersectionSceneBuffers& scene, const std::vector<GpuIntersectionRay>& rays) const {
    if (rays.empty()) {
      return {};
    }
    if (!scene.basicHitKernelEligible()) {
      throw std::invalid_argument(
        "Metal basic any-hit kernel requires a supported primitive scene");
    }

    @autoreleasepool {
      MetalWavefrontAnyHitKernelResult result;
      const auto uploadStart = std::chrono::steady_clock::now();
      id<MTLDevice> device = sharedMetalDevice();
      if (!device) {
        throw std::runtime_error("Metal wavefront basic any-hit kernel requires a Metal device");
      }

      id<MTLComputePipelineState> pipeline = sharedBasicAnyHitPipeline();

      id<MTLBuffer> bvhBuffer = newBufferFromVector(device, scene.bvh);
      id<MTLBuffer> primitiveBuffer = newBufferFromVector(device, scene.primitives);
      id<MTLBuffer> triangleBuffer = newPayloadBufferFromVector(device, scene.triangles);
      id<MTLBuffer> sphereBuffer = newPayloadBufferFromVector(device, scene.spheres);
      id<MTLBuffer> planeBuffer = newPayloadBufferFromVector(device, scene.planes);
      id<MTLBuffer> rectangleBuffer = newPayloadBufferFromVector(device, scene.rectangles);
      id<MTLBuffer> diskBuffer = newPayloadBufferFromVector(device, scene.disks);
      id<MTLBuffer> openCylinderBuffer = newPayloadBufferFromVector(device, scene.openCylinders);
      id<MTLBuffer> torusBuffer = newPayloadBufferFromVector(device, scene.tori);
      id<MTLBuffer> transformBuffer = newPayloadBufferFromVector(device, scene.transforms);
      id<MTLBuffer> rayBuffer = newBufferFromVector(device, rays);
      const std::vector<GpuIntersectionOcclusionRecord> initialRecords(rays.size());
      id<MTLBuffer> occlusionBuffer = newBufferFromVector(device, initialRecords);
      const std::array<std::uint32_t, 4> counts0{
        static_cast<std::uint32_t>(scene.bvh.size()),
        static_cast<std::uint32_t>(scene.primitives.size()),
        static_cast<std::uint32_t>(scene.triangles.size()),
        static_cast<std::uint32_t>(scene.spheres.size()),
      };
      const std::array<std::uint32_t, 4> counts1{
        static_cast<std::uint32_t>(scene.planes.size()),
        static_cast<std::uint32_t>(scene.rectangles.size()),
        static_cast<std::uint32_t>(scene.disks.size()),
        static_cast<std::uint32_t>(scene.openCylinders.size()),
      };
      const std::array<std::uint32_t, 4> counts2{
        static_cast<std::uint32_t>(rays.size()),
        static_cast<std::uint32_t>(scene.transforms.size()),
        static_cast<std::uint32_t>(scene.tori.size()),
        0u,
      };
      id<MTLBuffer> counts0Buffer =
        [device newBufferWithBytes:counts0.data()
                             length:counts0.size() * sizeof(std::uint32_t)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> counts1Buffer =
        [device newBufferWithBytes:counts1.data()
                             length:counts1.size() * sizeof(std::uint32_t)
                            options:MTLResourceStorageModeShared];
      id<MTLBuffer> counts2Buffer =
        [device newBufferWithBytes:counts2.data()
                             length:counts2.size() * sizeof(std::uint32_t)
                            options:MTLResourceStorageModeShared];
      if (!bvhBuffer || !primitiveBuffer || !triangleBuffer || !sphereBuffer || !planeBuffer ||
          !rectangleBuffer || !diskBuffer || !openCylinderBuffer || !torusBuffer ||
          !transformBuffer ||
          !rayBuffer || !occlusionBuffer || !counts0Buffer || !counts1Buffer || !counts2Buffer) {
        throw std::runtime_error("Metal wavefront basic any-hit buffer allocation failed");
      }

      id<MTLCommandQueue> queue = sharedCommandQueue();
      id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
      id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
      if (!queue || !commandBuffer || !encoder) {
        throw std::runtime_error("Metal wavefront basic any-hit command setup failed");
      }
      const auto uploadEnd = std::chrono::steady_clock::now();

      [encoder setComputePipelineState:pipeline];
      [encoder setBuffer:bvhBuffer offset:0 atIndex:0];
      [encoder setBuffer:primitiveBuffer offset:0 atIndex:1];
      [encoder setBuffer:triangleBuffer offset:0 atIndex:2];
      [encoder setBuffer:sphereBuffer offset:0 atIndex:3];
      [encoder setBuffer:planeBuffer offset:0 atIndex:4];
      [encoder setBuffer:rectangleBuffer offset:0 atIndex:5];
      [encoder setBuffer:diskBuffer offset:0 atIndex:6];
      [encoder setBuffer:openCylinderBuffer offset:0 atIndex:7];
      [encoder setBuffer:torusBuffer offset:0 atIndex:8];
      [encoder setBuffer:transformBuffer offset:0 atIndex:9];
      [encoder setBuffer:rayBuffer offset:0 atIndex:10];
      [encoder setBuffer:occlusionBuffer offset:0 atIndex:11];
      [encoder setBuffer:counts0Buffer offset:0 atIndex:12];
      [encoder setBuffer:counts1Buffer offset:0 atIndex:13];
      [encoder setBuffer:counts2Buffer offset:0 atIndex:14];
      dispatchOneDimensional(encoder, pipeline, rays.size());
      [encoder endEncoding];
      const auto kernelStart = std::chrono::steady_clock::now();
      [commandBuffer commit];
      [commandBuffer waitUntilCompleted];
      const auto kernelEnd = std::chrono::steady_clock::now();

      if (commandBuffer.error) {
        throw metalError("Metal wavefront basic any-hit kernel dispatch failed",
                         commandBuffer.error);
      }

      const auto readbackStart = std::chrono::steady_clock::now();
      result.records.resize(rays.size());
      std::memcpy(result.records.data(), occlusionBuffer.contents,
                  result.records.size() * sizeof(result.records.front()));
      const auto readbackEnd = std::chrono::steady_clock::now();
      result.timing.uploadSeconds = secondsBetween(uploadStart, uploadEnd);
      result.timing.kernelSeconds = secondsBetween(kernelStart, kernelEnd);
      result.timing.readbackSeconds = secondsBetween(readbackStart, readbackEnd);
      return result;
    }
  }
}
