#ifndef SURFACE_H
#define SURFACE_H

#include "world/objects/Element.h"
#include "core/math/Vector.h"
#include "core/math/Quaternion.h"

// TODO: move the scene creation to a separate factory class
namespace raytracer {
  class Primitive;
}

class Surface : public Element {
public:
  Surface();
  
  inline const Vector3d& position() const { return m_position; }
  inline void setPosition(const Vector3d& position) { m_position = position; }
  
  inline const Quaterniond& orientation() const { return m_orientation; }
  inline void setOrientation(const Quaterniond& orientation) { m_orientation = orientation; }
  
  inline bool visible() const { return m_visible; }
  inline void setVisible(bool visible) { m_visible = visible; }
  inline void show() { setVisible(true); }
  inline void hide() { setVisible(false); }
  
  inline const Vector3d& scale() const { return m_scale; }
  inline void setScale(const Vector3d& scale) { m_scale = scale; }
  
  virtual raytracer::Primitive* toRaytracerPrimitive() const = 0;
  
private:
  Vector3d m_position;
  Quaterniond m_orientation;
  
  bool m_visible;
  
  Vector3d m_scale;
  
//  Material* m_material;
};

#endif
