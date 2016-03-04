#pragma once

#include "world/objects/Surface.h"

class Intersection : public Surface {
  Q_OBJECT;
  Q_PROPERTY(bool active READ active WRITE setActive);
  
public:
  Intersection(Element* parent = nullptr);
  
  bool active() const { return m_active; }
  void setActive(bool active) { m_active = active; }

  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;
  
private:
  bool m_active;
};
