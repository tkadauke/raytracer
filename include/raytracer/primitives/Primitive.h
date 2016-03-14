#pragma once

#include "core/math/BoundingBox.h"
#include "core/math/Ray.h"

class HitPointInterval;

namespace raytracer {
  class Material;
  class State;

  class Primitive {
  public:
    Primitive() : m_material(nullptr) {}
    virtual ~Primitive() {}

    virtual Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) = 0;
    virtual bool intersects(const Rayd& ray, State& state);

    virtual BoundingBoxd boundingBox() = 0;

    inline void setMaterial(Material* material) {
      m_material = material;
    }
    
    inline virtual Material* material() const {
      return m_material;
    }
    
  private:
    Material* m_material;
  };
}
