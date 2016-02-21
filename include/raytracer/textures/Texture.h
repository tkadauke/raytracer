#pragma once

class Ray;
class HitPoint;

namespace raytracer {
  template<class T>
  class Texture {
  public:
    inline virtual ~Texture() {}
    
    virtual T evaluate(const Ray& ray, const HitPoint& hitPoint) const = 0;
  };
}
