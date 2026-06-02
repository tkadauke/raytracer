#pragma once

#include <memory>
#include <vector>

#include "render/primitives/Primitive.h"

class Mesh;

namespace core {
  class MeshAsset;
}

namespace render {
  class Material;

  /**
    * Runtime primitive for imported mesh geometry with owned mesh lifetime.
    *
    * `FlatMeshTriangle` and `SmoothMeshTriangle` are lightweight leaves that
    * point at mesh storage owned elsewhere. `MeshPrimitive` owns or shares that
    * storage and builds the triangle leaves internally, so imported geometry can
    * be passed through accelerators without dangling triangle pointers.
    */
  class MeshPrimitive : public Primitive {
  public:
    using Primitive::forEachLeaf;
    using Primitive::forEachLeafInBounds;

    /**
      * Optional material slots indexed by source mesh face. A quad or n-gon
      * face fans out to multiple triangle leaves that all receive the same
      * material slot.
      */
    using FaceMaterials = std::vector<std::shared_ptr<render::Material>>;

    enum class NormalMode { Flat, Smooth };

    explicit MeshPrimitive(Mesh mesh, NormalMode normalMode = NormalMode::Smooth);
    MeshPrimitive(Mesh mesh, FaceMaterials faceMaterials,
                  NormalMode normalMode = NormalMode::Smooth);
    explicit MeshPrimitive(std::shared_ptr<const Mesh> mesh,
                           NormalMode normalMode = NormalMode::Smooth);
    MeshPrimitive(std::shared_ptr<const Mesh> mesh, FaceMaterials faceMaterials,
                  NormalMode normalMode = NormalMode::Smooth);
    explicit MeshPrimitive(std::shared_ptr<const core::MeshAsset> asset,
                           NormalMode normalMode = NormalMode::Smooth);
    MeshPrimitive(std::shared_ptr<const core::MeshAsset> asset, FaceMaterials faceMaterials,
                  NormalMode normalMode = NormalMode::Smooth);

    const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                               render::State& state) const override;
    bool intersects(const Rayd& ray, render::State& state) const override;
    RayPacketIntersection4 intersectPacket(const Ray4& rays, render::State& state) const override;
    PrimitivePacketHit4 intersectPacketHits(const Ray4& rays,
                                            const PrimitivePacketState4& states) const override;
    PrimitivePacketHit8 intersectPacketHits(const Ray8& rays,
                                            const PrimitivePacketState8& states) const override;

    void forEachLeaf(std::shared_ptr<render::Material> inheritedMaterial,
                     const LeafVisitor& visitor) const override;
    void forEachLeafInBounds(const BoundsFilter& boundsFilter,
                             std::shared_ptr<render::Material> inheritedMaterial,
                             const LeafVisitor& visitor) const override;
    void forEachTransformedLeaf(std::shared_ptr<render::Material> inheritedMaterial,
                                const Matrix4d& pointMatrix, const Matrix3d& normalMatrix,
                                const TransformedLeafVisitor& visitor) const override;
    void forEachTransformedLeafInBounds(const BoundsFilter& boundsFilter,
                                        std::shared_ptr<render::Material> inheritedMaterial,
                                        const Matrix4d& pointMatrix, const Matrix3d& normalMatrix,
                                        const TransformedLeafVisitor& visitor) const override;

    std::shared_ptr<Mesh> tessellate(int lod = 0) const override;

    inline std::shared_ptr<const Mesh> mesh() const {
      return m_mesh;
    }

    inline std::shared_ptr<const core::MeshAsset> asset() const {
      return m_asset;
    }

    inline NormalMode normalMode() const {
      return m_normalMode;
    }

    inline const std::vector<std::shared_ptr<Primitive>>& leaves() const {
      return m_leaves;
    }

  protected:
    BoundingBoxd calculateBoundingBox() const override;

  private:
    void buildLeaves();
    [[nodiscard]] bool isBuildableTriangle(int index0, int index1, int index2) const;
    [[nodiscard]] bool hasValidVertexIndex(int index) const;
    std::shared_ptr<Primitive> buildLeaf(std::size_t faceIndex, int index0, int index1,
                                         int index2) const;
    std::shared_ptr<render::Material> materialForFace(std::size_t faceIndex) const;
    template<typename Packet, typename StateArray, typename Result>
    Result intersectPacketHitsFor(const Packet& rays, const StateArray& states) const;

    std::shared_ptr<const core::MeshAsset> m_asset;
    std::shared_ptr<const Mesh> m_mesh;
    FaceMaterials m_faceMaterials;
    NormalMode m_normalMode;
    std::vector<std::shared_ptr<Primitive>> m_leaves;
  };
}
