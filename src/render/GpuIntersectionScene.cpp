#include "render/GpuIntersectionScene.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

using namespace render;

namespace {
  template<typename T>
  constexpr bool isKernelRecord() {
    return std::is_standard_layout_v<T> && alignof(T) == 16 && sizeof(T) % 16 == 0;
  }

  static_assert(isKernelRecord<GpuIntersectionBounds>());
  static_assert(isKernelRecord<GpuIntersectionBvhNode>());
  static_assert(isKernelRecord<GpuIntersectionPrimitiveRecord>());
  static_assert(isKernelRecord<GpuIntersectionTrianglePayload>());
  static_assert(isKernelRecord<GpuIntersectionSpherePayload>());
  static_assert(isKernelRecord<GpuIntersectionPlanePayload>());
  static_assert(isKernelRecord<GpuIntersectionRectanglePayload>());
  static_assert(isKernelRecord<GpuIntersectionDiskPayload>());
  static_assert(isKernelRecord<GpuIntersectionTransformPayload>());
  static_assert(isKernelRecord<GpuIntersectionRay>());
  static_assert(isKernelRecord<GpuIntersectionHitRecord>());
  static_assert(isKernelRecord<GpuIntersectionOcclusionRecord>());
}

std::size_t GpuIntersectionSceneBuffers::uploadByteCount() const {
  return bvh.size() * sizeof(GpuIntersectionBvhNode) +
         primitives.size() * sizeof(GpuIntersectionPrimitiveRecord) +
         triangles.size() * sizeof(GpuIntersectionTrianglePayload) +
         spheres.size() * sizeof(GpuIntersectionSpherePayload) +
         planes.size() * sizeof(GpuIntersectionPlanePayload) +
         rectangles.size() * sizeof(GpuIntersectionRectanglePayload) +
         disks.size() * sizeof(GpuIntersectionDiskPayload) +
         transforms.size() * sizeof(GpuIntersectionTransformPayload);
}

bool GpuIntersectionSceneBuffers::triangleClosestHitKernelEligible() const {
  return !primitives.empty() &&
         std::all_of(primitives.begin(), primitives.end(), [this](const auto& primitive) {
           return primitive.kind ==
                    static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Triangle) &&
                  primitive.transform == 0 && primitiveHasBasicHitKernelTraversal(primitive);
         });
}

bool GpuIntersectionSceneBuffers::basicHitKernelEligible() const {
  return !primitives.empty() &&
         std::all_of(primitives.begin(), primitives.end(), [this](const auto& primitive) {
           return primitiveHasBasicHitKernelTraversal(primitive);
         });
}

bool GpuIntersectionSceneBuffers::packedClosestHitKernelEligible() const {
  return !primitives.empty() &&
         std::all_of(primitives.begin(), primitives.end(), [this](const auto& primitive) {
           return primitiveHasPackedClosestHitTraversal(primitive);
         });
}

bool GpuIntersectionSceneBuffers::packedAnyHitKernelEligible() const {
  return !primitives.empty() &&
         std::all_of(primitives.begin(), primitives.end(), [this](const auto& primitive) {
           return primitiveHasPackedAnyHitTraversal(primitive);
         });
}

bool GpuIntersectionSceneBuffers::primitiveHasBasicHitKernelTraversal(
  const GpuIntersectionPrimitiveRecord& primitive) const {
  if (primitive.payloadCount != 1) {
    return false;
  }

  if (primitive.transform != 0 && primitive.transform >= transforms.size()) {
    return false;
  }

  switch (static_cast<GpuIntersectionPrimitiveKind>(primitive.kind)) {
  case GpuIntersectionPrimitiveKind::Triangle:
    return primitive.payloadOffset < triangles.size();
  case GpuIntersectionPrimitiveKind::Sphere:
    return primitive.payloadOffset < spheres.size();
  case GpuIntersectionPrimitiveKind::Plane:
    return primitive.payloadOffset < planes.size();
  case GpuIntersectionPrimitiveKind::Rectangle:
    return primitive.payloadOffset < rectangles.size();
  case GpuIntersectionPrimitiveKind::Disk:
    return primitive.payloadOffset < disks.size();
  case GpuIntersectionPrimitiveKind::Unsupported:
    return false;
  }

  return false;
}

bool GpuIntersectionSceneBuffers::primitiveHasPackedClosestHitTraversal(
  const GpuIntersectionPrimitiveRecord& primitive) const {
  return primitiveHasBasicHitKernelTraversal(primitive);
}

bool GpuIntersectionSceneBuffers::primitiveHasPackedAnyHitTraversal(
  const GpuIntersectionPrimitiveRecord& primitive) const {
  return primitiveHasPackedClosestHitTraversal(primitive);
}

GpuIntersectionSceneBuffers
GpuIntersectionScenePacker::packScene(const CompiledIntersectionScene& scene) const {
  GpuIntersectionSceneBuffers buffers;
  buffers.bvh.reserve(scene.bvh().size());
  buffers.primitives.reserve(scene.primitives().size());
  buffers.triangles.reserve(scene.triangles().size());
  buffers.spheres.reserve(scene.spheres().size());
  buffers.planes.reserve(scene.planes().size());
  buffers.rectangles.reserve(scene.rectangles().size());
  buffers.disks.reserve(scene.disks().size());
  buffers.transforms.reserve(scene.transforms().size());

  for (const FlatIntersectionBvhNode& node : scene.bvh()) {
    buffers.bvh.push_back(packBvhNode(node));
  }
  for (const IntersectionPrimitiveRecord& primitive : scene.primitives()) {
    buffers.primitives.push_back(packPrimitiveRecord(primitive));
  }
  for (const IntersectionTrianglePayload& triangle : scene.triangles()) {
    buffers.triangles.push_back(packTrianglePayload(triangle));
  }
  for (const IntersectionSpherePayload& sphere : scene.spheres()) {
    buffers.spheres.push_back(packSpherePayload(sphere));
  }
  for (const IntersectionPlanePayload& plane : scene.planes()) {
    buffers.planes.push_back(packPlanePayload(plane));
  }
  for (const IntersectionRectanglePayload& rectangle : scene.rectangles()) {
    buffers.rectangles.push_back(packRectanglePayload(rectangle));
  }
  for (const IntersectionDiskPayload& disk : scene.disks()) {
    buffers.disks.push_back(packDiskPayload(disk));
  }
  for (const IntersectionTransformPayload& transform : scene.transforms()) {
    buffers.transforms.push_back(packTransformPayload(transform));
  }

  return buffers;
}

GpuIntersectionRay GpuIntersectionScenePacker::packRay(const Rayd& ray, std::uint32_t rayIndex,
                                                       double minDistance, double maxDistance,
                                                       double timeSample,
                                                       std::uint32_t flags) const {
  GpuIntersectionRay packed;
  packed.origin = packVector(ray.origin());
  packed.direction = packVector(ray.direction());
  packed.minDistance = packScalar(minDistance);
  packed.maxDistance = packScalar(maxDistance);
  packed.timeSample = packScalar(timeSample);
  packed.flags = flags;
  packed.rayIndex = rayIndex;
  return packed;
}

GpuIntersectionHitRecord GpuIntersectionScenePacker::packMiss(std::uint32_t rayIndex) const {
  GpuIntersectionHitRecord packed;
  packed.rayIndex = rayIndex;
  return packed;
}

float GpuIntersectionScenePacker::packScalar(double value) const {
  return static_cast<float>(value);
}

std::array<float, 4> GpuIntersectionScenePacker::packVector(const Vector2d& value) const {
  return {packScalar(value.x()), packScalar(value.y()), 0.0f, 0.0f};
}

std::array<float, 4> GpuIntersectionScenePacker::packVector(const Vector3d& value, float w) const {
  return {packScalar(value.x()), packScalar(value.y()), packScalar(value.z()), w};
}

std::array<float, 4> GpuIntersectionScenePacker::packVector(const Vector4d& value) const {
  return {packScalar(value.x()), packScalar(value.y()), packScalar(value.z()),
          packScalar(value.w())};
}

std::array<float, 16> GpuIntersectionScenePacker::packMatrix(const Matrix3d& value) const {
  std::array<float, 16> packed{};
  for (int row = 0; row != 3; ++row) {
    for (int column = 0; column != 3; ++column) {
      packed[static_cast<std::size_t>(row * 4 + column)] = packScalar(value[row][column]);
    }
  }
  packed[15] = 1.0f;
  return packed;
}

std::array<float, 16> GpuIntersectionScenePacker::packMatrix(const Matrix4d& value) const {
  std::array<float, 16> packed{};
  for (int row = 0; row != 4; ++row) {
    for (int column = 0; column != 4; ++column) {
      packed[static_cast<std::size_t>(row * 4 + column)] = packScalar(value[row][column]);
    }
  }
  return packed;
}

GpuIntersectionBounds GpuIntersectionScenePacker::packBounds(const BoundingBoxd& bounds) const {
  return GpuIntersectionBounds{packVector(bounds.min()), packVector(bounds.max())};
}

GpuIntersectionPrimitiveKind
GpuIntersectionScenePacker::packPrimitiveKind(IntersectionPrimitiveKind kind) const {
  static_assert(static_cast<std::uint32_t>(IntersectionPrimitiveKind::Unsupported) ==
                static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Unsupported));
  static_assert(static_cast<std::uint32_t>(IntersectionPrimitiveKind::Triangle) ==
                static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Triangle));
  static_assert(static_cast<std::uint32_t>(IntersectionPrimitiveKind::Sphere) ==
                static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Sphere));
  static_assert(static_cast<std::uint32_t>(IntersectionPrimitiveKind::Plane) ==
                static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Plane));
  static_assert(static_cast<std::uint32_t>(IntersectionPrimitiveKind::Rectangle) ==
                static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Rectangle));
  static_assert(static_cast<std::uint32_t>(IntersectionPrimitiveKind::Disk) ==
                static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Disk));
  return static_cast<GpuIntersectionPrimitiveKind>(static_cast<std::uint32_t>(kind));
}

GpuIntersectionBvhNode
GpuIntersectionScenePacker::packBvhNode(const FlatIntersectionBvhNode& node) const {
  return GpuIntersectionBvhNode{packBounds(node.bounds), node.leftOrFirstPrimitive,
                                node.primitiveCount, node.flags, 0};
}

GpuIntersectionPrimitiveRecord GpuIntersectionScenePacker::packPrimitiveRecord(
  const IntersectionPrimitiveRecord& primitive) const {
  GpuIntersectionPrimitiveRecord packed;
  packed.bounds = packBounds(primitive.bounds);
  packed.kind = static_cast<std::uint32_t>(packPrimitiveKind(primitive.kind));
  packed.material = primitive.material;
  packed.object = primitive.object;
  packed.transform = primitive.transform;
  packed.payloadOffset = primitive.payloadOffset;
  packed.payloadCount = primitive.payloadCount;
  return packed;
}

GpuIntersectionTrianglePayload
GpuIntersectionScenePacker::packTrianglePayload(const IntersectionTrianglePayload& payload) const {
  return GpuIntersectionTrianglePayload{
    packVector(payload.point0),  packVector(payload.point1),  packVector(payload.point2),
    packVector(payload.normal0), packVector(payload.normal1), packVector(payload.normal2),
    packVector(payload.uv0),     packVector(payload.uv1),     packVector(payload.uv2)};
}

GpuIntersectionSpherePayload
GpuIntersectionScenePacker::packSpherePayload(const IntersectionSpherePayload& payload) const {
  GpuIntersectionSpherePayload packed;
  packed.centerRadius = packVector(payload.center, packScalar(payload.radius));
  return packed;
}

GpuIntersectionPlanePayload
GpuIntersectionScenePacker::packPlanePayload(const IntersectionPlanePayload& payload) const {
  GpuIntersectionPlanePayload packed;
  packed.normalDistance = packVector(payload.normal, packScalar(payload.distance));
  return packed;
}

GpuIntersectionRectanglePayload GpuIntersectionScenePacker::packRectanglePayload(
  const IntersectionRectanglePayload& payload) const {
  return GpuIntersectionRectanglePayload{packVector(payload.corner), packVector(payload.leg1),
                                         packVector(payload.leg2), packVector(payload.normal)};
}

GpuIntersectionDiskPayload
GpuIntersectionScenePacker::packDiskPayload(const IntersectionDiskPayload& payload) const {
  return GpuIntersectionDiskPayload{packVector(payload.center, packScalar(payload.radius)),
                                    packVector(payload.normal)};
}

GpuIntersectionTransformPayload GpuIntersectionScenePacker::packTransformPayload(
  const IntersectionTransformPayload& payload) const {
  return GpuIntersectionTransformPayload{
    packMatrix(payload.pointMatrix), packMatrix(payload.normalMatrix),
    packMatrix(payload.inversePointMatrix), packMatrix(payload.inverseDirectionMatrix)};
}

GpuIntersectionHitRecord
GpuIntersectionIntersector::intersectClosest(const GpuIntersectionSceneBuffers& scene,
                                             const GpuIntersectionRay& ray) const {
  GpuIntersectionHitRecord closest = makeMiss(ray);
  if (scene.bvh.empty() || scene.primitives.empty()) {
    return closest;
  }

  std::vector<std::uint32_t> stack;
  stack.push_back(0);
  while (!stack.empty()) {
    const std::uint32_t nodeIndex = stack.back();
    stack.pop_back();
    if (nodeIndex >= scene.bvh.size()) {
      continue;
    }

    const GpuIntersectionBvhNode& node = scene.bvh[nodeIndex];
    if (!boundsIntersectRay(node.bounds, ray, closest.distance)) {
      continue;
    }

    if ((node.flags & gpuIntersectionLeafNodeFlag) == 0) {
      const std::uint32_t leftChild = node.leftOrFirstPrimitive;
      const std::uint32_t rightChild = node.primitiveCount;
      const std::optional<float> leftEntry =
        leftChild < scene.bvh.size()
          ? boundsRayEntryDistance(scene.bvh[leftChild].bounds, ray, closest.distance)
          : std::nullopt;
      const std::optional<float> rightEntry =
        rightChild < scene.bvh.size()
          ? boundsRayEntryDistance(scene.bvh[rightChild].bounds, ray, closest.distance)
          : std::nullopt;

      if (leftEntry && rightEntry) {
        if (*leftEntry <= *rightEntry) {
          stack.push_back(rightChild);
          stack.push_back(leftChild);
        } else {
          stack.push_back(leftChild);
          stack.push_back(rightChild);
        }
      } else if (leftEntry) {
        stack.push_back(leftChild);
      } else if (rightEntry) {
        stack.push_back(rightChild);
      }
      continue;
    }

    for (std::uint32_t offset = 0; offset != node.primitiveCount; ++offset) {
      const std::uint32_t primitiveIndex = node.leftOrFirstPrimitive + offset;
      if (primitiveIndex >= scene.primitives.size()) {
        continue;
      }

      const GpuIntersectionPrimitiveRecord& primitive = scene.primitives[primitiveIndex];
      if (!boundsIntersectRay(primitive.bounds, ray, closest.distance)) {
        continue;
      }

      const std::optional<ClosestHit> hit = intersectPrimitive(scene, ray, primitive);
      if (!hit) {
        continue;
      }

      if (!closest.hit || hit->distance < closest.distance) {
        closest = makeHit(ray, primitive, primitiveIndex, *hit);
      }
    }
  }

  return closest;
}

std::vector<GpuIntersectionHitRecord>
GpuIntersectionIntersector::intersectClosest(const GpuIntersectionSceneBuffers& scene,
                                             const std::vector<GpuIntersectionRay>& rays) const {
  std::vector<GpuIntersectionHitRecord> hits;
  hits.reserve(rays.size());
  for (const GpuIntersectionRay& ray : rays) {
    hits.push_back(intersectClosest(scene, ray));
  }
  return hits;
}

bool GpuIntersectionIntersector::intersectAny(const GpuIntersectionSceneBuffers& scene,
                                              const GpuIntersectionRay& ray) const {
  if (ray.maxDistance <= 0.0f || scene.bvh.empty() || scene.primitives.empty()) {
    return false;
  }

  std::vector<std::uint32_t> stack;
  stack.push_back(0);
  while (!stack.empty()) {
    const std::uint32_t nodeIndex = stack.back();
    stack.pop_back();
    if (nodeIndex >= scene.bvh.size()) {
      continue;
    }

    const GpuIntersectionBvhNode& node = scene.bvh[nodeIndex];
    if (!boundsIntersectRay(node.bounds, ray, ray.maxDistance)) {
      continue;
    }

    if ((node.flags & gpuIntersectionLeafNodeFlag) == 0) {
      stack.push_back(node.primitiveCount);
      stack.push_back(node.leftOrFirstPrimitive);
      continue;
    }

    for (std::uint32_t offset = 0; offset != node.primitiveCount; ++offset) {
      const std::uint32_t primitiveIndex = node.leftOrFirstPrimitive + offset;
      if (primitiveIndex >= scene.primitives.size()) {
        continue;
      }

      const GpuIntersectionPrimitiveRecord& primitive = scene.primitives[primitiveIndex];
      if (!boundsIntersectRay(primitive.bounds, ray, ray.maxDistance)) {
        continue;
      }

      const std::optional<ClosestHit> hit = intersectPrimitive(scene, ray, primitive);
      if (hit && hitOccludes(*hit, ray.maxDistance)) {
        return true;
      }
    }
  }

  return false;
}

bool GpuIntersectionIntersector::boundsIntersectRay(const GpuIntersectionBounds& bounds,
                                                    const GpuIntersectionRay& ray,
                                                    float maxHitDistance) const {
  return boundsRayEntryDistance(bounds, ray, maxHitDistance).has_value();
}

std::optional<float> GpuIntersectionIntersector::boundsRayEntryDistance(
  const GpuIntersectionBounds& bounds, const GpuIntersectionRay& ray, float maxHitDistance) const {
  float enter = ray.minDistance;
  float exit = std::min(ray.maxDistance, maxHitDistance);
  if (exit < enter) {
    return std::nullopt;
  }

  for (std::size_t axis = 0; axis != 3; ++axis) {
    const float origin = ray.origin[axis];
    const float direction = ray.direction[axis];
    const float minimum = bounds.minimum[axis];
    const float maximum = bounds.maximum[axis];
    if (std::abs(direction) <= std::numeric_limits<float>::epsilon()) {
      if (origin < minimum || origin > maximum) {
        return std::nullopt;
      }
      continue;
    }

    const float inverseDirection = 1.0f / direction;
    float nearDistance = (minimum - origin) * inverseDirection;
    float farDistance = (maximum - origin) * inverseDirection;
    if (nearDistance > farDistance) {
      std::swap(nearDistance, farDistance);
    }
    enter = std::max(enter, nearDistance);
    exit = std::min(exit, farDistance);
    if (exit < enter) {
      return std::nullopt;
    }
  }
  return enter;
}

bool GpuIntersectionIntersector::hitOccludes(const ClosestHit& hit, float maxDistance) const {
  if (std::isinf(maxDistance)) {
    return true;
  }

  const float occlusionLimit =
    std::max(0.0f, maxDistance - static_cast<float>(Rayd::epsilon * 4.0));
  return hit.distance < occlusionLimit;
}

std::optional<GpuIntersectionIntersector::ClosestHit> GpuIntersectionIntersector::intersectTriangle(
  const GpuIntersectionRay& ray, const GpuIntersectionTrianglePayload& triangle) const {
  const float a = triangle.point0[0] - triangle.point1[0];
  const float b = triangle.point0[0] - triangle.point2[0];
  const float c = ray.direction[0];
  const float d = triangle.point0[0] - ray.origin[0];
  const float e = triangle.point0[1] - triangle.point1[1];
  const float f = triangle.point0[1] - triangle.point2[1];
  const float g = ray.direction[1];
  const float h = triangle.point0[1] - ray.origin[1];
  const float i = triangle.point0[2] - triangle.point1[2];
  const float j = triangle.point0[2] - triangle.point2[2];
  const float k = ray.direction[2];
  const float l = triangle.point0[2] - ray.origin[2];

  const float m = f * k - g * j;
  const float n = h * k - g * l;
  const float p = f * l - h * j;
  const float q = g * i - e * k;
  const float r = e * l - h * i;
  const float s = e * j - f * i;

  const float denominator = a * m + b * q + c * s;
  if (denominator == 0.0f) {
    return std::nullopt;
  }

  const float invDenom = 1.0f / denominator;
  const float beta = (d * m - b * n - c * p) * invDenom;
  if (beta < 0.0f || beta > 1.0f) {
    return std::nullopt;
  }

  const float gamma = (a * n + d * q + c * r) * invDenom;
  if (gamma < 0.0f || gamma > 1.0f || beta + gamma > 1.0f) {
    return std::nullopt;
  }

  const float distance = (a * p - b * r + d * s) * invDenom;
  if (distance < ray.minDistance || distance > ray.maxDistance) {
    return std::nullopt;
  }

  const float alpha = 1.0f - beta - gamma;
  ClosestHit hit;
  hit.distance = distance;
  hit.point = {ray.origin[0] + ray.direction[0] * distance,
               ray.origin[1] + ray.direction[1] * distance,
               ray.origin[2] + ray.direction[2] * distance, 1.0f};
  hit.normal = normalize3(
    interpolate3(triangle.normal0, triangle.normal1, triangle.normal2, alpha, beta, gamma));
  hit.uv = interpolate3(triangle.uv0, triangle.uv1, triangle.uv2, alpha, beta, gamma);
  hit.barycentric = {alpha, beta, gamma, 0.0f};
  return hit;
}

std::optional<GpuIntersectionIntersector::ClosestHit>
GpuIntersectionIntersector::intersectSphere(const GpuIntersectionRay& ray,
                                            const GpuIntersectionSpherePayload& sphere) const {
  const float centerX = sphere.centerRadius[0];
  const float centerY = sphere.centerRadius[1];
  const float centerZ = sphere.centerRadius[2];
  const float radius = sphere.centerRadius[3];

  const float originX = ray.origin[0] - centerX;
  const float originY = ray.origin[1] - centerY;
  const float originZ = ray.origin[2] - centerZ;
  const float directionX = ray.direction[0];
  const float directionY = ray.direction[1];
  const float directionZ = ray.direction[2];
  const float od = originX * directionX + originY * directionY + originZ * directionZ;
  const float dd = directionX * directionX + directionY * directionY + directionZ * directionZ;
  if (dd <= std::numeric_limits<float>::epsilon()) {
    return std::nullopt;
  }

  const float originLengthSquared = originX * originX + originY * originY + originZ * originZ;
  const float discriminant = od * od - dd * (originLengthSquared - radius * radius);
  if (discriminant <= 0.0f) {
    return std::nullopt;
  }

  const float discriminantRoot = std::sqrt(discriminant);
  const float nearDistance = (-od - discriminantRoot) / dd;
  const float farDistance = (-od + discriminantRoot) / dd;
  if (nearDistance <= 0.0f && farDistance <= 0.0f) {
    return std::nullopt;
  }

  const float distance = nearDistance >= ray.minDistance ? nearDistance : farDistance;
  if (distance < ray.minDistance || distance > ray.maxDistance) {
    return std::nullopt;
  }

  ClosestHit hit;
  hit.distance = distance;
  hit.point = {ray.origin[0] + ray.direction[0] * distance,
               ray.origin[1] + ray.direction[1] * distance,
               ray.origin[2] + ray.direction[2] * distance, 1.0f};
  hit.normal =
    normalize3({hit.point[0] - centerX, hit.point[1] - centerY, hit.point[2] - centerZ, 0.0f});
  return hit;
}

std::optional<GpuIntersectionIntersector::ClosestHit>
GpuIntersectionIntersector::intersectPlane(const GpuIntersectionRay& ray,
                                           const GpuIntersectionPlanePayload& plane) const {
  const float normalX = plane.normalDistance[0];
  const float normalY = plane.normalDistance[1];
  const float normalZ = plane.normalDistance[2];
  const float planeDistance = plane.normalDistance[3];
  const float angle =
    normalX * ray.direction[0] + normalY * ray.direction[1] + normalZ * ray.direction[2];
  if (angle == 0.0f) {
    return std::nullopt;
  }

  const float distance =
    -(normalX * ray.origin[0] + normalY * ray.origin[1] + normalZ * ray.origin[2] + planeDistance) /
    angle;
  if (distance <= 0.0f || distance < ray.minDistance || distance > ray.maxDistance) {
    return std::nullopt;
  }

  ClosestHit hit;
  hit.distance = distance;
  hit.point = {ray.origin[0] + ray.direction[0] * distance,
               ray.origin[1] + ray.direction[1] * distance,
               ray.origin[2] + ray.direction[2] * distance, 1.0f};
  hit.normal = normalize3({normalX, normalY, normalZ, 0.0f});
  return hit;
}

std::optional<GpuIntersectionIntersector::ClosestHit>
GpuIntersectionIntersector::intersectRectangle(
  const GpuIntersectionRay& ray, const GpuIntersectionRectanglePayload& rectangle) const {
  const float normalX = rectangle.normal[0];
  const float normalY = rectangle.normal[1];
  const float normalZ = rectangle.normal[2];
  const float denominator =
    ray.direction[0] * normalX + ray.direction[1] * normalY + ray.direction[2] * normalZ;
  if (denominator == 0.0f) {
    return std::nullopt;
  }

  const float cornerToOriginX = rectangle.corner[0] - ray.origin[0];
  const float cornerToOriginY = rectangle.corner[1] - ray.origin[1];
  const float cornerToOriginZ = rectangle.corner[2] - ray.origin[2];
  const float distance =
    (cornerToOriginX * normalX + cornerToOriginY * normalY + cornerToOriginZ * normalZ) /
    denominator;
  if (!std::isfinite(distance) || distance < 0.0f || distance < ray.minDistance ||
      distance > ray.maxDistance) {
    return std::nullopt;
  }

  ClosestHit hit;
  hit.distance = distance;
  hit.point = {ray.origin[0] + ray.direction[0] * distance,
               ray.origin[1] + ray.direction[1] * distance,
               ray.origin[2] + ray.direction[2] * distance, 1.0f};

  const float differenceX = hit.point[0] - rectangle.corner[0];
  const float differenceY = hit.point[1] - rectangle.corner[1];
  const float differenceZ = hit.point[2] - rectangle.corner[2];
  const float dot1 = differenceX * rectangle.leg1[0] + differenceY * rectangle.leg1[1] +
                     differenceZ * rectangle.leg1[2];
  const float squaredLength1 = rectangle.leg1[0] * rectangle.leg1[0] +
                               rectangle.leg1[1] * rectangle.leg1[1] +
                               rectangle.leg1[2] * rectangle.leg1[2];
  if (dot1 < 0.0f || dot1 > squaredLength1) {
    return std::nullopt;
  }

  const float dot2 = differenceX * rectangle.leg2[0] + differenceY * rectangle.leg2[1] +
                     differenceZ * rectangle.leg2[2];
  const float squaredLength2 = rectangle.leg2[0] * rectangle.leg2[0] +
                               rectangle.leg2[1] * rectangle.leg2[1] +
                               rectangle.leg2[2] * rectangle.leg2[2];
  if (dot2 < 0.0f || dot2 > squaredLength2) {
    return std::nullopt;
  }

  hit.normal = normalize3({normalX, normalY, normalZ, 0.0f});
  return hit;
}

std::optional<GpuIntersectionIntersector::ClosestHit>
GpuIntersectionIntersector::intersectDisk(const GpuIntersectionRay& ray,
                                          const GpuIntersectionDiskPayload& disk) const {
  const float centerX = disk.centerRadius[0];
  const float centerY = disk.centerRadius[1];
  const float centerZ = disk.centerRadius[2];
  const float radius = disk.centerRadius[3];
  const float normalX = disk.normal[0];
  const float normalY = disk.normal[1];
  const float normalZ = disk.normal[2];
  const float denominator =
    ray.direction[0] * normalX + ray.direction[1] * normalY + ray.direction[2] * normalZ;
  if (denominator == 0.0f) {
    return std::nullopt;
  }

  const float distance =
    ((centerX - ray.origin[0]) * normalX + (centerY - ray.origin[1]) * normalY +
     (centerZ - ray.origin[2]) * normalZ) /
    denominator;
  if (!std::isfinite(distance) || distance < 0.0001f || distance < ray.minDistance ||
      distance > ray.maxDistance) {
    return std::nullopt;
  }

  ClosestHit hit;
  hit.distance = distance;
  hit.point = {ray.origin[0] + ray.direction[0] * distance,
               ray.origin[1] + ray.direction[1] * distance,
               ray.origin[2] + ray.direction[2] * distance, 1.0f};
  const float hitOffsetX = hit.point[0] - centerX;
  const float hitOffsetY = hit.point[1] - centerY;
  const float hitOffsetZ = hit.point[2] - centerZ;
  const float squaredDistance =
    hitOffsetX * hitOffsetX + hitOffsetY * hitOffsetY + hitOffsetZ * hitOffsetZ;
  if (!(squaredDistance < radius * radius)) {
    return std::nullopt;
  }

  hit.normal = normalize3({normalX, normalY, normalZ, 0.0f});
  return hit;
}

std::optional<GpuIntersectionIntersector::ClosestHit>
GpuIntersectionIntersector::intersectPrimitive(
  const GpuIntersectionSceneBuffers& scene, const GpuIntersectionRay& ray,
  const GpuIntersectionPrimitiveRecord& primitive) const {
  if (primitive.transform != 0) {
    if (primitive.transform >= scene.transforms.size()) {
      return std::nullopt;
    }

    const GpuIntersectionTransformPayload& transform = scene.transforms[primitive.transform];
    const std::optional<ClosestHit> hit =
      intersectPrimitivePayload(scene, transformRay(ray, transform), primitive);
    if (!hit) {
      return std::nullopt;
    }
    return transformHit(*hit, transform);
  }

  return intersectPrimitivePayload(scene, ray, primitive);
}

std::optional<GpuIntersectionIntersector::ClosestHit>
GpuIntersectionIntersector::intersectPrimitivePayload(
  const GpuIntersectionSceneBuffers& scene, const GpuIntersectionRay& ray,
  const GpuIntersectionPrimitiveRecord& primitive) const {
  if (primitive.kind == static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Triangle)) {
    if (primitive.payloadOffset >= scene.triangles.size()) {
      return std::nullopt;
    }
    return intersectTriangle(ray, scene.triangles[primitive.payloadOffset]);
  }

  if (primitive.kind == static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Sphere)) {
    if (primitive.payloadOffset >= scene.spheres.size()) {
      return std::nullopt;
    }
    return intersectSphere(ray, scene.spheres[primitive.payloadOffset]);
  }

  if (primitive.kind == static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Plane)) {
    if (primitive.payloadOffset >= scene.planes.size()) {
      return std::nullopt;
    }
    return intersectPlane(ray, scene.planes[primitive.payloadOffset]);
  }

  if (primitive.kind == static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Rectangle)) {
    if (primitive.payloadOffset >= scene.rectangles.size()) {
      return std::nullopt;
    }
    return intersectRectangle(ray, scene.rectangles[primitive.payloadOffset]);
  }

  if (primitive.kind == static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Disk)) {
    if (primitive.payloadOffset >= scene.disks.size()) {
      return std::nullopt;
    }
    return intersectDisk(ray, scene.disks[primitive.payloadOffset]);
  }

  return std::nullopt;
}

GpuIntersectionHitRecord
GpuIntersectionIntersector::makeHit(const GpuIntersectionRay& ray,
                                    const GpuIntersectionPrimitiveRecord& primitive,
                                    std::uint32_t primitiveRecord, const ClosestHit& hit) const {
  GpuIntersectionHitRecord record;
  record.hit = 1;
  record.material = primitive.material;
  record.object = primitive.object;
  record.primitiveRecord = primitiveRecord;
  record.rayIndex = ray.rayIndex;
  record.distance = hit.distance;
  record.point = hit.point;
  record.normal = hit.normal;
  record.uv = hit.uv;
  record.barycentric = hit.barycentric;
  return record;
}

GpuIntersectionHitRecord GpuIntersectionIntersector::makeMiss(const GpuIntersectionRay& ray) const {
  return GpuIntersectionScenePacker().packMiss(ray.rayIndex);
}

std::array<float, 4> GpuIntersectionIntersector::interpolate3(const std::array<float, 4>& a,
                                                              const std::array<float, 4>& b,
                                                              const std::array<float, 4>& c,
                                                              float alpha, float beta,
                                                              float gamma) const {
  return {a[0] * alpha + b[0] * beta + c[0] * gamma, a[1] * alpha + b[1] * beta + c[1] * gamma,
          a[2] * alpha + b[2] * beta + c[2] * gamma, a[3] * alpha + b[3] * beta + c[3] * gamma};
}

GpuIntersectionRay
GpuIntersectionIntersector::transformRay(const GpuIntersectionRay& ray,
                                         const GpuIntersectionTransformPayload& transform) const {
  GpuIntersectionRay transformed = ray;
  transformed.origin = transformPoint(transform.inversePointMatrix, ray.origin);
  transformed.direction = transformDirection(transform.inverseDirectionMatrix, ray.direction);
  return transformed;
}

GpuIntersectionIntersector::ClosestHit
GpuIntersectionIntersector::transformHit(const ClosestHit& hit,
                                         const GpuIntersectionTransformPayload& transform) const {
  ClosestHit transformed = hit;
  transformed.point = transformPoint(transform.pointMatrix, hit.point);
  transformed.normal = normalize3(transformDirection(transform.normalMatrix, hit.normal));
  return transformed;
}

std::array<float, 4>
GpuIntersectionIntersector::transformPoint(const std::array<float, 16>& matrix,
                                           const std::array<float, 4>& point) const {
  return {
    matrix[0] * point[0] + matrix[1] * point[1] + matrix[2] * point[2] + matrix[3] * point[3],
    matrix[4] * point[0] + matrix[5] * point[1] + matrix[6] * point[2] + matrix[7] * point[3],
    matrix[8] * point[0] + matrix[9] * point[1] + matrix[10] * point[2] + matrix[11] * point[3],
    matrix[12] * point[0] + matrix[13] * point[1] + matrix[14] * point[2] + matrix[15] * point[3]};
}

std::array<float, 4>
GpuIntersectionIntersector::transformDirection(const std::array<float, 16>& matrix,
                                               const std::array<float, 4>& direction) const {
  return {matrix[0] * direction[0] + matrix[1] * direction[1] + matrix[2] * direction[2],
          matrix[4] * direction[0] + matrix[5] * direction[1] + matrix[6] * direction[2],
          matrix[8] * direction[0] + matrix[9] * direction[1] + matrix[10] * direction[2], 0.0f};
}

std::array<float, 4> GpuIntersectionIntersector::normalize3(std::array<float, 4> value) const {
  const float squaredLength = value[0] * value[0] + value[1] * value[1] + value[2] * value[2];
  if (squaredLength <= 0.0f) {
    return value;
  }

  const float scale = 1.0f / std::sqrt(squaredLength);
  value[0] *= scale;
  value[1] *= scale;
  value[2] *= scale;
  return value;
}
