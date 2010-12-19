#ifndef INSTANCE_H
#define INSTANCE_H

#include "surfaces/Surface.h"
#include "math/Matrix.h"
#include "math/Ray.h"

class Instance : public Surface {
public:
  Instance(Surface* surface) : m_surface(surface) {}
  virtual ~Instance() { }
  
  Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);
  bool intersects(const Ray& ray);
  
  void setMatrix(const Matrix4d& matrix);

private:
  inline Ray instancedRay(const Ray& ray) const {
    return Ray(m_originMatrix * ray.origin(), m_directionMatrix * ray.direction());
  }
  
  Surface* m_surface;
  Matrix4d m_pointMatrix;
  Matrix4d m_originMatrix;
  Matrix3d m_directionMatrix;
  Matrix3d m_normalMatrix;
};

#endif
