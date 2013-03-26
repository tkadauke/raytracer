#ifndef SPHERE_H
#define SPHERE_H

#include "world/objects/Surface.h"

class Sphere : public Surface {
public:
  Sphere();
  
  inline double radius() const { return m_radius; }
  inline void setRadius(double radius) { m_radius = radius; }

  virtual raytracer::Primitive* toRaytracerPrimitive() const;

private:
  double m_radius;
};

#endif
