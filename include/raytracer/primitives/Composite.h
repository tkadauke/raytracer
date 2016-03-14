#pragma once

#include <list>

#include "raytracer/primitives/Primitive.h"

namespace raytracer {
  class Composite : public Primitive {
  public:
    typedef std::list<std::shared_ptr<Primitive>> Primitives;

    inline Composite() {}
    ~Composite();

    virtual Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state);
    virtual bool intersects(const Rayd& ray, State& state);
    virtual BoundingBoxd boundingBox();

    inline void add(std::shared_ptr<Primitive> primitive) {
      m_primitives.push_back(primitive);
    }

    inline const Primitives& primitives() const {
      return m_primitives;
    }

  private:
    Primitives m_primitives;
  };
}
