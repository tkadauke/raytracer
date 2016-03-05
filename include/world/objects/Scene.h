#pragma once

#include "world/objects/Element.h"
#include "core/Color.h"

class Camera;

namespace raytracer {
  class Scene;
}

class Scene : public Element {
  Q_OBJECT;
  Q_PROPERTY(Colord ambient READ ambient WRITE setAmbient);
  Q_PROPERTY(Colord background READ background WRITE setBackground);
  
public:
  Scene(Element* parent = nullptr);
  
  raytracer::Scene* toRaytracerScene() const;
  
  bool save(const QString& filename);
  bool load(const QString& filename);
  
  inline bool changed() const {
    return m_changed;
  }
  
  inline void setChanged(bool changed) {
    m_changed = changed;
  }
  
  inline Colord ambient() const {
    return m_ambient;
  }
  
  inline void setAmbient(const Colord& ambient) {
    m_ambient = ambient;
  }
  
  inline const Colord& background() const {
    return m_background;
  }
  
  inline void setBackground(const Colord& background) {
    m_background = background;
  }
  
  Camera* activeCamera() const;
  virtual bool canHaveChild(Element* child) const;
  
private:
  void findReferences(Element* root, QMap<QString, Element*>& references);
  
  bool m_changed;
  Colord m_ambient;
  Colord m_background;
};
