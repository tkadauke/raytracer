#pragma once

class HitPoint;

namespace raytracer {
  class TextureMapping2D {
  public:
    virtual ~TextureMapping2D() {}
    
    virtual void map(const HitPoint& hitPoint, double& s, double& t) const = 0;
  };
}
