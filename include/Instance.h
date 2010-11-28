#ifndef INSTANCE_H
#define INSTANCE_H

#include "Surface.h"
#include "Matrix.h"

class Instance : public Surface {
public:
  Instance(Surface* surface) : m_surface(surface) {}
  virtual ~Instance() { }
  
  Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);
  
  void setMatrix(const Matrix4d& matrix);

private:
  Surface* m_surface;
  Matrix4d m_pointMatrix;
  Matrix4d m_originMatrix;
  Matrix3d m_directionMatrix;
  Matrix3d m_normalMatrix;
};

#endif
