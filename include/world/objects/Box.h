#ifndef BOX_H
#define BOX_H

#include "world/objects/Surface.h"

class Box : public Surface {
  Q_OBJECT
  Q_PROPERTY(Vector3d size READ size WRITE setSize);
  
public:
  Box(Element* parent);
  
  inline const Vector3d& size() const { return m_size; }
  inline void setSize(const Vector3d& size) { m_size = size; }

  virtual raytracer::Primitive* toRaytracerPrimitive() const;

private:
  Vector3d m_size;
};

#endif
