#pragma once
#include <memory>

#include "world/objects/Transformable.h"

namespace render {
  class Primitive;
  class Scene;
}

/**
  * Organizes scene objects without adding geometry of its own.
  *
  * A Group can contain surfaces, lights, and other groups. Surface and nested
  * group geometry is converted into a render::Composite wrapped in this group's
  * transform; lights are registered with the runtime scene using their global
  * transform.
  */
class Group : public Transformable {
  Q_OBJECT
  Q_PROPERTY(bool visible READ visible WRITE setVisible)

public:
  /**
    * Default constructor.
    */
  explicit Group(Element* parent = nullptr);

  /**
    * @returns true if the group and its descendants are visible.
    */
  inline bool visible() const {
    return m_visible;
  }

  /**
    * Sets the group's visibility property.
    */
  inline void setVisible(bool visible) {
    m_visible = visible;
  }

  /**
    * Sets the group's visible flag to true.
    */
  inline void show() {
    setVisible(true);
  }

  /**
    * Sets the group's visible flag to false.
    */
  inline void hide() {
    setVisible(false);
  }

  /**
    * Converts visible child geometry into a transformed runtime composite.
    */
  std::shared_ptr<render::Primitive> toRaytracer(render::Scene* scene) const;
  virtual bool canHaveChild(Element* child) const;

private:
  std::shared_ptr<render::Primitive>
  applyTransform(std::shared_ptr<render::Primitive> primitive) const;

  bool m_visible;
};
