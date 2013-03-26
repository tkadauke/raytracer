#ifndef RAYTRACER_BOX_H
#define RAYTRACER_BOX_H

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Box : public Primitive {
  public:
    Box(const Vector3d& center, const Vector3d& edge)
      : m_center(center), m_edge(edge)
    {
    }

    virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);
    virtual BoundingBox boundingBox();

  private:
    Vector3d m_center, m_edge;
  };
}

#endif
