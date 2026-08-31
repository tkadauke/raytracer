#pragma once

#include "core/math/BoundingBox.h"
#include "core/math/Matrix.h"
#include "core/math/Ray.h"
#include "core/math/Vector.h"
#include "render/GpuFloat4.h"
#include "render/IntersectionSceneCompiler.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

namespace render {

  template<typename T>
  constexpr bool isKernelRecord() {
    return std::is_standard_layout_v<T> && alignof(T) == 16 && sizeof(T) % 16 == 0;
  }

  enum class GpuIntersectionPrimitiveKind : std::uint32_t {
    Unsupported = 0,
    Triangle = 1,
    Sphere = 2,
    Plane = 3,
    Rectangle = 4,
    Disk = 5,
    OpenCylinder = 6,
    Torus = 7
  };

  inline constexpr std::uint32_t gpuIntersectionLeafNodeFlag = 1u;

  struct alignas(16) GpuIntersectionBounds {
    GpuFloat4 minimum{};
    GpuFloat4 maximum{};
  };

  struct alignas(16) GpuIntersectionBvhNode {
    GpuIntersectionBounds bounds;
    std::uint32_t leftOrFirstPrimitive{0};
    std::uint32_t primitiveCount{0};
    std::uint32_t flags{0};
    std::uint32_t reserved{0};
  };

  struct alignas(16) GpuIntersectionPrimitiveRecord {
    GpuIntersectionBounds bounds;
    std::uint32_t kind{0};
    std::uint32_t material{0};
    std::uint32_t object{0};
    std::uint32_t transform{0};
    std::uint32_t payloadOffset{0};
    std::uint32_t payloadCount{0};
    std::array<std::uint32_t, 2> reserved{};
  };

  struct alignas(16) GpuIntersectionTrianglePayload {
    GpuFloat4 point0{};
    GpuFloat4 point1{};
    GpuFloat4 point2{};
    GpuFloat4 normal0{};
    GpuFloat4 normal1{};
    GpuFloat4 normal2{};
    GpuFloat4 uv0{};
    GpuFloat4 uv1{};
    GpuFloat4 uv2{};
    GpuFloat4 minimumHitDistance{};
  };

  struct alignas(16) GpuIntersectionSpherePayload {
    GpuFloat4 centerRadius{};
  };

  struct alignas(16) GpuIntersectionPlanePayload {
    GpuFloat4 normalDistance{};
  };

  struct alignas(16) GpuIntersectionRectanglePayload {
    GpuFloat4 corner{};
    GpuFloat4 leg1{};
    GpuFloat4 leg2{};
    GpuFloat4 normal{};
  };

  struct alignas(16) GpuIntersectionDiskPayload {
    GpuFloat4 centerRadius{};
    GpuFloat4 normalMinimumHitDistance{};
  };

  struct alignas(16) GpuIntersectionOpenCylinderPayload {
    GpuFloat4 radiusHalfHeight{};
  };

  struct alignas(16) GpuIntersectionTorusPayload {
    GpuFloat4 sweptTubeRadius{};
  };

  struct alignas(16) GpuIntersectionTransformPayload {
    std::array<float, 16> pointMatrix{};
    std::array<float, 16> normalMatrix{};
    std::array<float, 16> inversePointMatrix{};
    std::array<float, 16> inverseDirectionMatrix{};
    GpuFloat4 motionDelta{};
  };

  struct alignas(16) GpuIntersectionRay {
    GpuFloat4 origin{};
    GpuFloat4 direction{};
    float minDistance{0.0f};
    float maxDistance{std::numeric_limits<float>::infinity()};
    float timeSample{0.0f};
    std::uint32_t flags{0};
    std::uint32_t rayIndex{0};
    std::array<std::uint32_t, 3> reserved{};
  };

  struct alignas(16) GpuIntersectionHitRecord {
    std::uint32_t hit{0};
    std::uint32_t material{0};
    std::uint32_t object{0};
    std::uint32_t primitiveRecord{0};
    std::uint32_t rayIndex{0};
    std::array<std::uint32_t, 3> reservedIds{};
    float distance{std::numeric_limits<float>::infinity()};
    std::array<float, 3> reservedDistance{};
    GpuFloat4 point{};
    GpuFloat4 normal{};
    GpuFloat4 uv{};
    GpuFloat4 barycentric{};
  };

  struct alignas(16) GpuIntersectionOcclusionRecord {
    std::uint32_t occluded{0};
    std::uint32_t rayIndex{0};
    std::array<std::uint32_t, 2> reserved{};
  };

  struct GpuIntersectionSceneBuffers {
    std::vector<GpuIntersectionBvhNode> bvh;
    std::vector<GpuIntersectionPrimitiveRecord> primitives;
    std::vector<GpuIntersectionTrianglePayload> triangles;
    std::vector<GpuIntersectionSpherePayload> spheres;
    std::vector<GpuIntersectionPlanePayload> planes;
    std::vector<GpuIntersectionRectanglePayload> rectangles;
    std::vector<GpuIntersectionDiskPayload> disks;
    std::vector<GpuIntersectionOpenCylinderPayload> openCylinders;
    std::vector<GpuIntersectionTorusPayload> tori;
    std::vector<GpuIntersectionTransformPayload> transforms;

    [[nodiscard]] std::size_t uploadByteCount() const;
    [[nodiscard]] bool triangleClosestHitKernelEligible() const;
    [[nodiscard]] bool basicHitKernelEligible() const;
    [[nodiscard]] bool packedClosestHitKernelEligible() const;
    [[nodiscard]] bool packedAnyHitKernelEligible() const;
    [[nodiscard]] bool
    primitiveHasBasicHitKernelTraversal(const GpuIntersectionPrimitiveRecord& primitive) const;
    [[nodiscard]] bool
    primitiveHasPackedClosestHitTraversal(const GpuIntersectionPrimitiveRecord& primitive) const;
    [[nodiscard]] bool
    primitiveHasPackedAnyHitTraversal(const GpuIntersectionPrimitiveRecord& primitive) const;
  };

  class GpuIntersectionScenePacker {
  public:
    [[nodiscard]] GpuIntersectionSceneBuffers
    packScene(const CompiledIntersectionScene& scene) const;
    [[nodiscard]] GpuIntersectionRay
    packRay(const Rayd& ray, std::uint32_t rayIndex, double minDistance = 0.0,
            double maxDistance = std::numeric_limits<double>::infinity(), double timeSample = 0.0,
            std::uint32_t flags = 0) const;
    [[nodiscard]] GpuIntersectionHitRecord packMiss(std::uint32_t rayIndex) const;

  private:
    [[nodiscard]] float packScalar(double value) const;
    [[nodiscard]] GpuIntersectionBounds packBounds(const BoundingBoxd& bounds) const;
    [[nodiscard]] GpuIntersectionPrimitiveKind
    packPrimitiveKind(IntersectionPrimitiveKind kind) const;
    [[nodiscard]] GpuIntersectionBvhNode packBvhNode(const FlatIntersectionBvhNode& node) const;
    [[nodiscard]] GpuIntersectionPrimitiveRecord
    packPrimitiveRecord(const IntersectionPrimitiveRecord& primitive) const;
    [[nodiscard]] GpuIntersectionTrianglePayload
    packTrianglePayload(const IntersectionTrianglePayload& payload) const;
    [[nodiscard]] GpuIntersectionSpherePayload
    packSpherePayload(const IntersectionSpherePayload& payload) const;
    [[nodiscard]] GpuIntersectionPlanePayload
    packPlanePayload(const IntersectionPlanePayload& payload) const;
    [[nodiscard]] GpuIntersectionRectanglePayload
    packRectanglePayload(const IntersectionRectanglePayload& payload) const;
    [[nodiscard]] GpuIntersectionDiskPayload
    packDiskPayload(const IntersectionDiskPayload& payload) const;
    [[nodiscard]] GpuIntersectionOpenCylinderPayload
    packOpenCylinderPayload(const IntersectionOpenCylinderPayload& payload) const;
    [[nodiscard]] GpuIntersectionTorusPayload
    packTorusPayload(const IntersectionTorusPayload& payload) const;
    [[nodiscard]] GpuIntersectionTransformPayload
    packTransformPayload(const IntersectionTransformPayload& payload) const;
  };

  class GpuIntersectionIntersector {
  public:
    [[nodiscard]] GpuIntersectionHitRecord
    intersectClosest(const GpuIntersectionSceneBuffers& scene, const GpuIntersectionRay& ray) const;
    [[nodiscard]] std::vector<GpuIntersectionHitRecord>
    intersectClosest(const GpuIntersectionSceneBuffers& scene,
                     const std::vector<GpuIntersectionRay>& rays) const;
    [[nodiscard]] bool intersectAny(const GpuIntersectionSceneBuffers& scene,
                                    const GpuIntersectionRay& ray) const;
    [[nodiscard]] std::vector<GpuIntersectionOcclusionRecord>
    intersectAny(const GpuIntersectionSceneBuffers& scene,
                 const std::vector<GpuIntersectionRay>& rays) const;

  private:
    struct ClosestHit {
      float distance{std::numeric_limits<float>::infinity()};
      GpuFloat4 point{};
      GpuFloat4 normal{};
      GpuFloat4 uv{};
      GpuFloat4 barycentric{};
    };

    [[nodiscard]] bool boundsIntersectRay(const GpuIntersectionBounds& bounds,
                                          const GpuIntersectionRay& ray,
                                          float maxHitDistance) const;
    [[nodiscard]] std::optional<float> boundsRayEntryDistance(const GpuIntersectionBounds& bounds,
                                                              const GpuIntersectionRay& ray,
                                                              float maxHitDistance) const;
    [[nodiscard]] bool hitOccludes(const ClosestHit& hit, float maxDistance) const;
    [[nodiscard]] std::optional<ClosestHit>
    intersectTriangle(const GpuIntersectionRay& ray,
                      const GpuIntersectionTrianglePayload& triangle) const;
    [[nodiscard]] std::optional<ClosestHit>
    intersectSphere(const GpuIntersectionRay& ray,
                    const GpuIntersectionSpherePayload& sphere) const;
    [[nodiscard]] std::optional<ClosestHit>
    intersectPlane(const GpuIntersectionRay& ray, const GpuIntersectionPlanePayload& plane) const;
    [[nodiscard]] std::optional<ClosestHit>
    intersectRectangle(const GpuIntersectionRay& ray,
                       const GpuIntersectionRectanglePayload& rectangle) const;
    [[nodiscard]] std::optional<ClosestHit>
    intersectDisk(const GpuIntersectionRay& ray, const GpuIntersectionDiskPayload& disk) const;
    [[nodiscard]] std::optional<ClosestHit>
    intersectOpenCylinder(const GpuIntersectionRay& ray,
                          const GpuIntersectionOpenCylinderPayload& openCylinder) const;
    [[nodiscard]] std::optional<ClosestHit>
    intersectTorus(const GpuIntersectionRay& ray, const GpuIntersectionTorusPayload& torus) const;
    [[nodiscard]] std::optional<ClosestHit>
    intersectPrimitive(const GpuIntersectionSceneBuffers& scene, const GpuIntersectionRay& ray,
                       const GpuIntersectionPrimitiveRecord& primitive) const;
    [[nodiscard]] std::optional<ClosestHit>
    intersectPrimitivePayload(const GpuIntersectionSceneBuffers& scene,
                              const GpuIntersectionRay& ray,
                              const GpuIntersectionPrimitiveRecord& primitive) const;
    [[nodiscard]] GpuIntersectionHitRecord makeHit(const GpuIntersectionRay& ray,
                                                   const GpuIntersectionPrimitiveRecord& primitive,
                                                   std::uint32_t primitiveRecord,
                                                   const ClosestHit& hit) const;
    [[nodiscard]] GpuIntersectionHitRecord makeMiss(const GpuIntersectionRay& ray) const;
    [[nodiscard]] GpuIntersectionOcclusionRecord makeOcclusionRecord(const GpuIntersectionRay& ray,
                                                                     bool occluded) const;
    [[nodiscard]] GpuFloat4 interpolate3(const GpuFloat4& a, const GpuFloat4& b, const GpuFloat4& c,
                                         float alpha, float beta, float gamma) const;
    [[nodiscard]] GpuIntersectionRay
    transformRay(const GpuIntersectionRay& ray,
                 const GpuIntersectionTransformPayload& transform) const;
    [[nodiscard]] ClosestHit transformHit(const ClosestHit& hit,
                                          const GpuIntersectionTransformPayload& transform,
                                          float timeSample) const;
    [[nodiscard]] GpuFloat4 normalize3(GpuFloat4 value) const;
  };
}
