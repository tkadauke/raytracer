#pragma once

#include "core/Color.h"
#include "core/math/Ray.h"

#include "render/Object.h"

class HitPoint;

namespace render {
  template<class T>
  class Texture : public render::Object {
  public:
    inline virtual ~Texture() {}
    
    virtual T evaluate(const Rayd& ray, const HitPoint& hitPoint) const = 0;
  };

  typedef Texture<Colord> Texturec;
}
