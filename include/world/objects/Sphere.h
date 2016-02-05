#ifndef SPHERE_H
#define SPHERE_H

#include "world/objects/Surface.h"

class Sphere : public Surface {
  Q_OBJECT
  Q_PROPERTY(double radius READ radius WRITE setRadius);
  
public:
  Sphere(Element* parent);
  
  inline double radius() const { return m_radius; }
  inline void setRadius(double radius) { m_radius = radius; }

  virtual raytracer::Primitive* toRaytracerPrimitive() const;

private:
  double m_radius;
};

#endif
