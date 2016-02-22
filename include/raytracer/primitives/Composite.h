#pragma once

#include <list>

#include "raytracer/primitives/Primitive.h"

namespace raytracer {
  class Composite : public Primitive {
  public:
    typedef std::list<std::shared_ptr<Primitive>> Primitives;

    inline Composite() {}
    ~Composite();

    virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);
    virtual bool intersects(const Ray& ray);
    virtual BoundingBox boundingBox();

    inline void add(std::shared_ptr<Primitive> primitive) { m_primitives.push_back(primitive); }

    inline const Primitives& primitives() const { return m_primitives; }

  private:
    Primitives m_primitives;
  };
}
