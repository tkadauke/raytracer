#pragma once

#include "core/math/BoundingBox.h"

class Ray;
class HitPointInterval;

namespace raytracer {
  class Material;

  class Primitive {
  public:
    Primitive() : m_material(nullptr) {}
    virtual ~Primitive() {}

    virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints) = 0;
    virtual bool intersects(const Ray& ray);

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
