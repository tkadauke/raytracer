#ifndef RAYTRACER_PRIMITIVE_H
#define RAYTRACER_PRIMITIVE_H

#include "core/math/BoundingBox.h"

class Ray;
class HitPointInterval;

namespace raytracer {
  class Composite;
  class Material;

  class Primitive {
  public:
    Primitive() : m_parent(0), m_material(0) {}
    virtual ~Primitive() {}

    virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints) = 0;
    virtual bool intersects(const Ray& ray);

    virtual BoundingBox boundingBox() = 0;

    inline void setMaterial(Material* material) { m_material = material; }
    inline virtual Material* material() const { return m_material; }

    inline void setParent(Composite* parent) { m_parent = parent; }
    inline Composite* parent() const { return m_parent; }

  protected:
    Composite* m_parent;

  private:
    Material* m_material;
  };
}

#endif
