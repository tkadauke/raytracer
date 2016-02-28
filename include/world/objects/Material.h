#pragma once

#include "world/objects/Element.h"

namespace raytracer {
  class Material;
}

class Material : public Element {
  Q_OBJECT;

public:
  static Material* defaultMaterial();
  
  Material(Element* parent = nullptr);
  
  virtual raytracer::Material* toRaytracerMaterial() const = 0;
};
