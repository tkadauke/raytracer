#pragma once

#include "world/objects/Element.h"
#include "core/math/Vector.h"
#include "core/math/Quaternion.h"

// TODO: move the scene creation to a separate factory class
namespace raytracer {
  class Primitive;
}

class Material;

class Surface : public Element {
  Q_OBJECT
  Q_PROPERTY(bool visible READ visible WRITE setVisible);
  Q_PROPERTY(Vector3d position READ position WRITE setPosition);
  Q_PROPERTY(Vector3d rotation READ rotation WRITE setRotation);
  Q_PROPERTY(Vector3d scale READ scale WRITE setScale);
  Q_PROPERTY(Material* material READ material WRITE setMaterial);
  
public:
  Surface(Element* parent = nullptr);
  
  inline const Vector3d& position() const { return m_position; }
  inline void setPosition(const Vector3d& position) { m_position = position; }

  inline const Vector3d& rotation() const { return m_rotation; }
  inline void setRotation(const Vector3d& rotation) { m_rotation = rotation; }
  
  inline const Vector3d& scale() const { return m_scale; }
  inline void setScale(const Vector3d& scale) {
    m_scale = Vector3d(
      std::max(std::abs(scale.x()), std::numeric_limits<double>::epsilon()),
      std::max(std::abs(scale.y()), std::numeric_limits<double>::epsilon()),
      std::max(std::abs(scale.z()), std::numeric_limits<double>::epsilon())
    );
  }
  
  inline bool visible() const { return m_visible; }
  inline void setVisible(bool visible) { m_visible = visible; }
  inline void show() { setVisible(true); }
  inline void hide() { setVisible(false); }
  
  inline Material* material() const { return m_material; }
  inline void setMaterial(Material* material) { m_material = material; }

  std::shared_ptr<raytracer::Primitive> toRaytracer() const;
  
protected:
  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const = 0;
  std::shared_ptr<raytracer::Primitive> applyTransform(std::shared_ptr<raytracer::Primitive> primitive) const;
  
private:
  Vector3d m_position;
  Vector3d m_rotation;
  Vector3d m_scale;
  
  Material* m_material;
  
  bool m_visible;
};
