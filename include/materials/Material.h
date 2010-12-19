#ifndef MATERIAL_H
#define MATERIAL_H

#include "Color.h"
#include "math/Vector.h"

class Raytracer;
class HitPoint;
class Ray;

class Material {
public:
  virtual ~Material() {}
  
  virtual Colord shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth) = 0;
};

#endif
