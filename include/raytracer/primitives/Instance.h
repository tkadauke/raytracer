#pragma once

#include "raytracer/primitives/Primitive.h"
#include "core/math/Matrix.h"
#include "core/math/Ray.h"

namespace raytracer {
  class Instance : public Primitive {
  public:
    Instance(std::shared_ptr<Primitive> primitive) : m_primitive(primitive) {}
    virtual ~Instance() { }

    Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints);
    bool intersects(const Rayd& ray);

    virtual Material* material() const;

    virtual BoundingBoxd boundingBox();

    void setMatrix(const Matrix4d& matrix);

  private:
    inline Rayd instancedRay(const Rayd& ray) const {
      return Rayd(m_originMatrix * ray.origin(), m_directionMatrix * ray.direction());
    }

    std::shared_ptr<Primitive> m_primitive;
    Matrix4d m_pointMatrix;
    Matrix4d m_originMatrix;
    Matrix3d m_directionMatrix;
    Matrix3d m_normalMatrix;
  };
}
