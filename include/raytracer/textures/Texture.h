#pragma once

class Ray;
class HitPoint;

namespace raytracer {
  template<class T>
  class Texture {
  public:
    virtual T evaluate(const Ray& ray, const HitPoint& hitPoint) const = 0;
  };
}
