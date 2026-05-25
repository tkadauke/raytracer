#pragma once

#include <memory>
#include <vector>

#include "render/primitives/Primitive.h"

class Mesh;

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

    enum class NormalMode { Flat, Smooth };

    explicit MeshPrimitive(Mesh mesh, NormalMode normalMode = NormalMode::Smooth);
    explicit MeshPrimitive(std::shared_ptr<const Mesh> mesh,
                           NormalMode normalMode = NormalMode::Smooth);

    const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                               render::State& state) const override;
    bool intersects(const Rayd& ray, render::State& state) const override;
    RayPacketIntersection4 intersectPacket(const Ray4& rays, render::State& state) const override;

    void forEachLeaf(std::shared_ptr<render::Material> inheritedMaterial,
                     const LeafVisitor& visitor) const override;
    void forEachLeafInBounds(const BoundsFilter& boundsFilter,
                             std::shared_ptr<render::Material> inheritedMaterial,
                             const LeafVisitor& visitor) const override;

    std::shared_ptr<Mesh> tessellate(int lod = 0) const override;

    inline std::shared_ptr<const Mesh> mesh() const {
      return m_mesh;
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

    std::shared_ptr<const Mesh> m_mesh;
    NormalMode m_normalMode;
    std::vector<std::shared_ptr<Primitive>> m_leaves;
  };
}
