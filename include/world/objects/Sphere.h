#pragma once

#include "world/objects/Surface.h"

class Sphere : public Surface {
  Q_OBJECT;
  Q_PROPERTY(double radius READ radius WRITE setRadius);
  
public:
  Sphere(Element* parent = nullptr);
  
  inline double radius() const {
    return m_radius;
  }
  
  inline void setRadius(double radius) {
    m_radius = radius;
  }

  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;
  
private:
  double m_radius;
};
