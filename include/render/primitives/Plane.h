#pragma once

#include "render/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace render {
  class Plane : public Primitive {
  public:
    inline explicit Plane(const Vector3d& normal, double distance)
        : m_normal(normal),
          m_distance(distance) {
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                       render::State& state) const override;
    RayPacketIntersection4 intersectPacket(const Ray4& rays, render::State& state) const override;
    PrimitivePacketHit4 intersectPacketHits(const Ray4& rays,
                                            const PrimitivePacketState4& states) const override;
    virtual bool intersects(const Rayd& ray, render::State& state) const override;

    /**
      * Plane is infinite and cannot be tessellated without first clipping it to
      * a finite region. Returns an empty Mesh and emits a warning. To obtain a
      * mesh, replace this Plane with a Rectangle and tessellate that instead.
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod) const override;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const override;

  private:
    double calculateIntersectionDistance(const Rayd& ray) const;

    Vector3d m_normal;
    double m_distance;
  };
}
