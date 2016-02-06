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
  Q_OBJECT
  Q_PROPERTY(Vector3d position READ position WRITE setPosition);
  Q_PROPERTY(Vector3d rotation READ rotation WRITE setRotation);
  Q_PROPERTY(Vector3d scale READ scale WRITE setScale);
  Q_PROPERTY(bool visible READ visible WRITE setVisible);
  
public:
  Surface(Element* parent);
  
  inline const Vector3d& position() const { return m_position; }
  inline void setPosition(const Vector3d& position) { m_position = position; }

  inline const Vector3d& rotation() const { return m_rotation; }
  inline void setRotation(const Vector3d& rotation) { m_rotation = rotation; }
  
  inline const Vector3d& scale() const { return m_scale; }
  inline void setScale(const Vector3d& scale) { m_scale = scale; }
  
  inline bool visible() const { return m_visible; }
  inline void setVisible(bool visible) { m_visible = visible; }
  inline void show() { setVisible(true); }
  inline void hide() { setVisible(false); }
  
  virtual raytracer::Primitive* toRaytracerPrimitive() const = 0;

protected:
  raytracer::Primitive* applyTransform(raytracer::Primitive* primitive) const;
  
private:
  Vector3d m_position;
  Vector3d m_rotation;
  Vector3d m_scale;
  
  bool m_visible;
};

#endif
