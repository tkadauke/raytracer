#pragma once

#include "render/primitives/Composite.h"

namespace render {
  /**
    * Composite primitive that subtracts later children's ray-hit intervals from
    * the first child. Boundaries contributed by the subtracted shape flip their
    * normals so lighting sees the exposed interior surface. The interval-set
    * operation is illustrated in HitPointInterval.
    *
    * @see HitPointInterval
    */
  class Difference : public Composite {
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
    virtual bool intersects(const Rayd& ray, render::State& state) const override;
    void
    appendIntersectionSceneRecords(IntersectionSceneBuilder& builder,
                                   std::shared_ptr<render::Material> inheritedMaterial,
                                   const Matrix4d& pointMatrix, const Matrix3d& normalMatrix,
                                   const Primitive* inheritedObject = nullptr,
                                   const Vector3d& motionDelta = Vector3d::null) const override;

    /** CSG mesh booleans are not implemented. Returns empty Mesh. */
    virtual std::shared_ptr<Mesh> tessellate(int lod) const override;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const override;

  private:
    template<typename Packet, typename StateArray, typename Result>
    Result intersectPacketIntervalsFor(const Packet& rays, const StateArray& states) const;
  };
}
