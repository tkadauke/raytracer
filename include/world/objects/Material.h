#pragma once

#include "world/objects/Element.h"

namespace raytracer {
  class Material;
}

class Material : public Element {
  Q_OBJECT

public:
  Material(Element* parent);
  
  virtual raytracer::Material* toRaytracerMaterial() const = 0;
};
