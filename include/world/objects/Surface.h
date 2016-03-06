#pragma once

#include "world/objects/Transformable.h"

namespace raytracer {
  class Primitive;
  class Scene;
}

class Material;

class Surface : public Transformable {
  Q_OBJECT;
  Q_PROPERTY(bool visible READ visible WRITE setVisible);
  Q_PROPERTY(Material* material READ material WRITE setMaterial);
  
public:
  Surface(Element* parent = nullptr);
  
  inline bool visible() const {
    return m_visible;
  }
  
  inline void setVisible(bool visible) {
    m_visible = visible;
  }
  
  inline void show() {
    setVisible(true);
  }
  
  inline void hide() {
    setVisible(false);
  }
  
  inline Material* material() const {
    return m_material;
  }
  
  inline void setMaterial(Material* material) {
    m_material = material;
  }
  
  std::shared_ptr<raytracer::Primitive> toRaytracer(raytracer::Scene* scene) const;
  
  virtual bool canHaveChild(Element* child) const;
  
protected:
  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const = 0;
  std::shared_ptr<raytracer::Primitive> applyTransform(std::shared_ptr<raytracer::Primitive> primitive) const;
  
private:
  Material* m_material;
  
  bool m_visible;
};
