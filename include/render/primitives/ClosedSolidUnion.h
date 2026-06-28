#pragma once

#include "render/primitives/Composite.h"

namespace render {
  class ClosedSolidUnion : public Composite {
  public:
    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                       render::State& state) const override;
    PrimitivePacketHit4 intersectPacketHits(const Ray4& rays,
                                            const PrimitivePacketState4& states) const override;
    PrimitivePacketHit8 intersectPacketHits(const Ray8& rays,
                                            const PrimitivePacketState8& states) const override;
    PrimitivePacketInterval4
    intersectPacketIntervals(const Ray4& rays, const PrimitivePacketState4& states) const override;
    PrimitivePacketInterval8
    intersectPacketIntervals(const Ray8& rays, const PrimitivePacketState8& states) const override;
    void appendIntersectionSceneRecords(IntersectionSceneBuilder& builder,
                                        std::shared_ptr<render::Material> inheritedMaterial,
                                        const Matrix4d& pointMatrix, const Matrix3d& normalMatrix,
                                        const Primitive* inheritedObject = nullptr) const override;
    virtual Vector3d farthestPoint(const Vector3d& direction) const override;

  private:
    template<typename Packet, typename StateArray, typename Result>
    Result intersectPacketIntervalsFor(const Packet& rays, const StateArray& states) const;
  };
}
