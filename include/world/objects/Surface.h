#pragma once

#include "world/objects/Transformable.h"

namespace raytracer {
  class Primitive;
  class Scene;
}

class Material;

/**
  * Abstract base class for visible objects. All Surfaces have a transformation
  * and material.
  */
class Surface : public Transformable {
  Q_OBJECT;
  Q_PROPERTY(bool visible READ visible WRITE setVisible);
  Q_PROPERTY(Material* material READ material WRITE setMaterial);
  
public:
  /**
    * Default constructor.
    */
  explicit Surface(Element* parent = nullptr);
  
  /**
    * @returns true if the surface is visible, false otherwise.
    */
  inline bool visible() const {
    return m_visible;
  }
  
  /**
    * Sets the surface's visibility property to visible.
    */
  inline void setVisible(bool visible) {
    m_visible = visible;
  }
  
  /**
    * Sets the surface's visible flag to true.
    */
  inline void show() {
    setVisible(true);
  }
  
  /**
    * Sets the surface's visible flag to false.
    */
  inline void hide() {
    setVisible(false);
  }
  
  /**
    * @returns the surface's material.
    */
  inline Material* material() const {
    return m_material;
  }
  
  /**
    * Sets the surface's material.
    */
  inline void setMaterial(Material* material) {
    m_material = material;
  }
  
  /**
    * Converts this surface into a raytracer::Primitive.
    */
  std::shared_ptr<raytracer::Primitive> toRaytracer(raytracer::Scene* scene) const;
  virtual bool canHaveChild(Element* child) const;
  
protected:
  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const = 0;
  std::shared_ptr<raytracer::Primitive> applyTransform(std::shared_ptr<raytracer::Primitive> primitive) const;
  
private:
  Material* m_material;
  
  bool m_visible;
};
