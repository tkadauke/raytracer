#pragma once

#include "world/objects/Element.h"

namespace raytracer {
  class Material;
}

/**
  * Abstract base class for materials.
  */
class Material : public Element {
  Q_OBJECT;

public:
  /**
    * Returns the default material, which is a black matte material.
    */
  static Material* defaultMaterial();
  
  /**
    * Default constructor.
    */
  explicit Material(Element* parent = nullptr);
  
  /**
    * Converts this material to the corresponding class in the raytracer
    * namespace.
    */
  virtual raytracer::Material* toRaytracerMaterial() const = 0;
};
