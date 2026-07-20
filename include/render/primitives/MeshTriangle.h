#pragma once

#include <array>

#include "core/geometry/MeshFaceMetadata.h"
#include "render/primitives/Primitive.h"

class Mesh;

namespace render {
  class MeshTriangle : public Primitive {
  public:
    inline explicit MeshTriangle(const Mesh* mesh, int index0, int index1, int index2)
        : MeshTriangle(mesh, index0, index1, index2, core::MeshFaceMetadata()) {
    }

    inline MeshTriangle(const Mesh* mesh, int index0, int index1, int index2,
                        core::MeshFaceMetadata faceMetadata)
        : Primitive(),
          m_mesh(mesh),
          m_index0(index0),
          m_index1(index1),
          m_index2(index2),
          m_faceMetadata(faceMetadata) {
    }

    RayPacketIntersection4 intersectPacket(const Ray4& rays, render::State& state) const override;
    PrimitivePacketHit4 intersectPacketHits(const Ray4& rays,
                                            const PrimitivePacketState4& states) const override;
    PrimitivePacketHit8 intersectPacketHits(const Ray8& rays,
                                            const PrimitivePacketState8& states) const override;

    [[nodiscard]] const core::MeshFaceMetadata& faceMetadata() const {
      return m_faceMetadata;
    }
    void appendIntersectionSceneRecord(IntersectionSceneBuilder& builder,
                                       const TransformedLeaf& leaf) const override;
    [[nodiscard]] bool requiresClosedSolidUnionCsgWhenOverlapped() const override {
      return false;
    }

  protected:
    virtual BoundingBoxd calculateBoundingBox() const override;
    virtual Vector3d normalAtBarycentric(double beta, double gamma) const = 0;
    virtual double minimumHitDistance() const;

    Vector2d uvAtBarycentric(double beta, double gamma) const;

  protected:
    const Mesh* m_mesh;
    int m_index0, m_index1, m_index2;
    core::MeshFaceMetadata m_faceMetadata;

  private:
    struct PacketBarycentricIntersection4 {
      int hitMask = 0;
      alignas(16) std::array<float, Ray4::lanes> distances{};
      alignas(16) std::array<float, Ray4::lanes> betas{};
      alignas(16) std::array<float, Ray4::lanes> gammas{};
    };

    PacketBarycentricIntersection4 intersectPacketBarycentric(const Ray4& rays) const;
    HitPoint materializeHitPoint(const Rayd& ray, double distance, double beta, double gamma) const;
  };
}
