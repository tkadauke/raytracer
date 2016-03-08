#pragma once

#include "core/Color.h"
#include "core/math/Ray.h"

class HitPoint;

namespace raytracer {
  template<class T>
  class Texture {
  public:
    inline virtual ~Texture() {}
    
    virtual T evaluate(const Rayd& ray, const HitPoint& hitPoint) const = 0;
  };

  typedef Texture<Colord> Texturec;
}
