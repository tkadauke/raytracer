#ifndef INSTANCE_H
#define INSTANCE_H

#include "primitives/Primitive.h"
#include "math/Matrix.h"
#include "math/Ray.h"

class Instance : public Primitive {
public:
  Instance(Primitive* primitive) : m_primitive(primitive) {}
  virtual ~Instance() { }
  
  Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);
  bool intersects(const Ray& ray);
  
  virtual BoundingBox boundingBox();

  void setMatrix(const Matrix4d& matrix);

private:
  inline Ray instancedRay(const Ray& ray) const {
    return Ray(m_originMatrix * ray.origin(), m_directionMatrix * ray.direction());
  }
  
  Primitive* m_primitive;
  Matrix4d m_pointMatrix;
  Matrix4d m_originMatrix;
  Matrix3d m_directionMatrix;
  Matrix3d m_normalMatrix;
};

#endif
