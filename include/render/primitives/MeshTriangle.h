#pragma once

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

    [[nodiscard]] const core::MeshFaceMetadata& faceMetadata() const {
      return m_faceMetadata;
    }

  protected:
    virtual BoundingBoxd calculateBoundingBox() const override;

  protected:
    const Mesh* m_mesh;
    int m_index0, m_index1, m_index2;
    core::MeshFaceMetadata m_faceMetadata;
  };
}
