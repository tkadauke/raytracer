#ifndef PORTAL_MATERIAL_H
#define PORTAL_MATERIAL_H

#include "materials/Material.h"
#include "math/Matrix.h"
#include "math/Ray.h"

class PortalMaterial : public Material {
public:
  inline PortalMaterial(const Matrix4d& transformation, const Colord& filter)
    : m_filterColor(filter)
  {
    setMatrix(transformation);
  }
  
  void setMatrix(const Matrix4d& matrix);
  
  virtual Colord shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth);

private:
  inline Ray transformedRay(const Ray& ray) const {
    return Ray(m_originMatrix * ray.origin(), m_directionMatrix * ray.direction());
  }
  
  Matrix4d m_originMatrix;
  Matrix3d m_directionMatrix;
  Colord m_filterColor;
};

#endif
