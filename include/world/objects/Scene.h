#pragma once

#include "world/objects/Element.h"

class Camera;

namespace raytracer {
  class Scene;
}

class Scene : public Element {
  Q_OBJECT;
  
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
  
  Camera* activeCamera() const;
  
private:
  void findReferences(Element* root, QMap<QString, Element*>& references);
  
  bool m_changed;
};
