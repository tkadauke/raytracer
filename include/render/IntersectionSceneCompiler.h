#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/math/BoundingBox.h"
#include "core/math/Matrix.h"
#include "core/math/Ray.h"
#include "core/math/Vector.h"
#include "render/primitives/Primitive.h"

namespace render {
  class Material;
  class Scene;

  enum class IntersectionPrimitiveKind : std::uint32_t {
    Unsupported = 0,
    Triangle = 1,
    Sphere = 2,
    Plane = 3,
    Rectangle = 4,
    Disk = 5
  };

  using IntersectionMaterialId = std::uint32_t;
  using IntersectionObjectId = std::uint32_t;
  using IntersectionTransformId = std::uint32_t;

  struct IntersectionPrimitiveRecord {
    IntersectionPrimitiveKind kind{IntersectionPrimitiveKind::Unsupported};
    IntersectionMaterialId material{0};
    IntersectionObjectId object{0};
    IntersectionTransformId transform{0};
    BoundingBoxd bounds;
    std::uint32_t payloadOffset{0};
    std::uint32_t payloadCount{0};
  };

  struct IntersectionTrianglePayload {
    Vector3d point0;
    Vector3d point1;
    Vector3d point2;
    Vector3d normal0;
    Vector3d normal1;
    Vector3d normal2;
    Vector2d uv0;
    Vector2d uv1;
    Vector2d uv2;
  };

  struct IntersectionSpherePayload {
    Vector3d center;
    double radius{0.0};
  };

  struct IntersectionPlanePayload {
    Vector3d normal;
    double distance{0.0};
  };

  struct IntersectionRectanglePayload {
    Vector3d corner;
    Vector3d leg1;
    Vector3d leg2;
    Vector3d normal;
  };

  struct IntersectionDiskPayload {
    Vector3d center;
    Vector3d normal;
    double radius{0.0};
  };

  struct IntersectionTransformPayload {
    Matrix4d pointMatrix;
    Matrix3d normalMatrix;
    Matrix4d inversePointMatrix;
    Matrix3d inverseDirectionMatrix;
  };

  struct FlatIntersectionBvhNode {
    [[nodiscard]] static FlatIntersectionBvhNode
    leaf(const BoundingBoxd& bounds, std::uint32_t firstPrimitive, std::uint32_t primitiveCount);
    [[nodiscard]] static FlatIntersectionBvhNode
    branch(const BoundingBoxd& bounds, std::uint32_t leftChild, std::uint32_t rightChild);

    [[nodiscard]] bool isLeaf() const;
    [[nodiscard]] std::uint32_t firstPrimitive() const;
    [[nodiscard]] std::uint32_t leafPrimitiveCount() const;
    [[nodiscard]] std::uint32_t leftChild() const;
    [[nodiscard]] std::uint32_t rightChild() const;

    BoundingBoxd bounds;
    std::uint32_t leftOrFirstPrimitive{0};
    std::uint32_t primitiveCount{0};
    std::uint32_t flags{0};
  };

  struct UnsupportedIntersectionPrimitive {
    IntersectionObjectId object{0};
    std::string primitiveName;
    std::string reason;
  };

  struct CompiledIntersectionHit {
    bool hit{false};
    IntersectionMaterialId material{0};
    IntersectionObjectId object{0};
    std::uint32_t primitiveRecord{0};
    double distance{0.0};
    Vector4d point;
    Vector3d normal;
    Vector2d uv;
    Vector3d barycentric;
  };

  class CompiledIntersectionScene {
  public:
    [[nodiscard]] bool fullySupported() const;
    [[nodiscard]] BoundingBoxd bounds() const;

    [[nodiscard]] const std::vector<FlatIntersectionBvhNode>& bvh() const;
    [[nodiscard]] const std::vector<IntersectionPrimitiveRecord>& primitives() const;
    [[nodiscard]] const std::vector<IntersectionTrianglePayload>& triangles() const;
    [[nodiscard]] const std::vector<IntersectionSpherePayload>& spheres() const;
    [[nodiscard]] const std::vector<IntersectionPlanePayload>& planes() const;
    [[nodiscard]] const std::vector<IntersectionRectanglePayload>& rectangles() const;
    [[nodiscard]] const std::vector<IntersectionDiskPayload>& disks() const;
    [[nodiscard]] const std::vector<IntersectionTransformPayload>& transforms() const;
    [[nodiscard]] const std::vector<std::shared_ptr<Material>>& materials() const;
    [[nodiscard]] const std::vector<const Primitive*>& objects() const;
    [[nodiscard]] const std::vector<UnsupportedIntersectionPrimitive>&
    unsupportedPrimitives() const;

  private:
    friend class IntersectionSceneBuilder;

    std::vector<FlatIntersectionBvhNode> m_bvh;
    std::vector<IntersectionPrimitiveRecord> m_primitives;
    std::vector<IntersectionTrianglePayload> m_triangles;
    std::vector<IntersectionSpherePayload> m_spheres;
    std::vector<IntersectionPlanePayload> m_planes;
    std::vector<IntersectionRectanglePayload> m_rectangles;
    std::vector<IntersectionDiskPayload> m_disks;
    std::vector<IntersectionTransformPayload> m_transforms;
    std::vector<std::shared_ptr<Material>> m_materials;
    std::vector<const Primitive*> m_objects;
    std::vector<UnsupportedIntersectionPrimitive> m_unsupportedPrimitives;
  };

  class IntersectionSceneBuilder {
  public:
    void addUnsupportedPrimitive(const Primitive::TransformedLeaf& leaf, std::string reason);
    void addTriangle(const Primitive::TransformedLeaf& leaf,
                     const IntersectionTrianglePayload& payload);
    void addTriangle(const Primitive::TransformedLeaf& leaf,
                     const IntersectionTrianglePayload& payload, const BoundingBoxd& bounds);
    void addSphere(const Primitive::TransformedLeaf& leaf, const Vector3d& center, double radius);
    void addPlane(const Primitive::TransformedLeaf& leaf, const Vector3d& normal, double distance);
    void addRectangle(const Primitive::TransformedLeaf& leaf, const Vector3d& corner,
                      const Vector3d& leg1, const Vector3d& leg2, const Vector3d& normal);
    void addDisk(const Primitive::TransformedLeaf& leaf, const Vector3d& center,
                 const Vector3d& normal, double radius);

    [[nodiscard]] CompiledIntersectionScene finish();

  private:
    IntersectionMaterialId materialIdFor(std::shared_ptr<Material> material);
    IntersectionObjectId objectIdFor(const Primitive* primitive);
    IntersectionTransformId transformIdFor(const Primitive::TransformedLeaf& leaf);
    void addPrimitive(const Primitive::TransformedLeaf& leaf, IntersectionPrimitiveKind kind,
                      std::uint32_t payloadOffset, std::uint32_t payloadCount);
    void addPrimitive(const Primitive::TransformedLeaf& leaf, IntersectionPrimitiveKind kind,
                      const BoundingBoxd& bounds, std::uint32_t payloadOffset,
                      std::uint32_t payloadCount);

    CompiledIntersectionScene m_scene;
    std::map<const Material*, IntersectionMaterialId> m_materialIds;
    std::map<const Primitive*, IntersectionObjectId> m_objectIds;
  };

  class IntersectionSceneCompiler {
  public:
    [[nodiscard]] CompiledIntersectionScene compile(const Scene& scene) const;
  };

  class CompiledIntersectionSceneIntersector {
  public:
    [[nodiscard]] CompiledIntersectionHit intersectClosest(const CompiledIntersectionScene& scene,
                                                           const Rayd& ray) const;
    [[nodiscard]] bool intersectAny(const CompiledIntersectionScene& scene, const Rayd& ray,
                                    double maxDistance) const;

  private:
    struct PrimitiveSpaceRay {
      Rayd ray;
      const IntersectionTransformPayload* transform{nullptr};
    };

    [[nodiscard]] std::optional<CompiledIntersectionHit>
    intersectPrimitive(const CompiledIntersectionScene& scene,
                       const IntersectionPrimitiveRecord& primitive, const Rayd& ray) const;
    [[nodiscard]] bool boundsIntersectRay(const BoundingBoxd& bounds, const Rayd& ray,
                                          double maxDistance) const;
    [[nodiscard]] std::optional<double>
    boundsRayEntryDistance(const BoundingBoxd& bounds, const Rayd& ray, double maxDistance) const;
    void pushIntersectingChildren(const CompiledIntersectionScene& scene,
                                  const FlatIntersectionBvhNode& node, const Rayd& ray,
                                  double maxDistance, std::vector<std::uint32_t>& stack) const;
    [[nodiscard]] bool hitOccludes(const CompiledIntersectionHit& hit, double maxDistance) const;
    [[nodiscard]] std::optional<PrimitiveSpaceRay>
    rayForPrimitive(const CompiledIntersectionScene& scene,
                    const IntersectionPrimitiveRecord& primitive, const Rayd& ray) const;
    [[nodiscard]] Vector4d hitPointForPrimitive(const PrimitiveSpaceRay& primitiveRay,
                                                const Vector4d& point) const;
    [[nodiscard]] Vector3d hitNormalForPrimitive(const PrimitiveSpaceRay& primitiveRay,
                                                 const Vector3d& normal) const;
    [[nodiscard]] CompiledIntersectionHit
    makeHit(const IntersectionPrimitiveRecord& primitive, const PrimitiveSpaceRay& primitiveRay,
            double distance, const Vector4d& localPoint, const Vector3d& localNormal,
            const Vector2d& uv = Vector2d::null,
            const Vector3d& barycentric = Vector3d::null) const;
    [[nodiscard]] std::optional<CompiledIntersectionHit>
    intersectTriangle(const CompiledIntersectionScene& scene,
                      const IntersectionPrimitiveRecord& primitive, const Rayd& ray) const;
    [[nodiscard]] std::optional<CompiledIntersectionHit>
    intersectSphere(const CompiledIntersectionScene& scene,
                    const IntersectionPrimitiveRecord& primitive, const Rayd& ray) const;
    [[nodiscard]] std::optional<CompiledIntersectionHit>
    intersectPlane(const CompiledIntersectionScene& scene,
                   const IntersectionPrimitiveRecord& primitive, const Rayd& ray) const;
    [[nodiscard]] std::optional<CompiledIntersectionHit>
    intersectRectangle(const CompiledIntersectionScene& scene,
                       const IntersectionPrimitiveRecord& primitive, const Rayd& ray) const;
    [[nodiscard]] std::optional<CompiledIntersectionHit>
    intersectDisk(const CompiledIntersectionScene& scene,
                  const IntersectionPrimitiveRecord& primitive, const Rayd& ray) const;
  };

  [[nodiscard]] const char* toString(IntersectionPrimitiveKind kind);
}
