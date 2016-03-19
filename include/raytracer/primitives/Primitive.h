#pragma once

#include "core/math/BoundingBox.h"
#include "core/math/Ray.h"
#include "core/MemoizedValue.h"

class HitPointInterval;

namespace raytracer {
  class Material;
  class State;

  class Primitive {
  public:
    Primitive() : m_material(nullptr) {}
    virtual ~Primitive() {}

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const = 0;
    virtual bool intersects(const Rayd& ray, State& state) const;

    virtual BoundingBoxd boundingBox() const = 0;

    inline void setMaterial(Material* material) {
      m_material = material;
    }
    
    inline virtual Material* material() const {
      return m_material;
    }
    
  protected:
    inline bool boundingBoxIntersects(const Rayd& ray) const {
      if (!m_cachedBoundingBox) {
        m_cachedBoundingBox = boundingBox();
      }
      
      return m_cachedBoundingBox.value().intersects(ray);
    }
    
  private:
    Material* m_material;
    mutable MemoizedValue<BoundingBoxd> m_cachedBoundingBox;
  };
}
