#include "raytracer/materials/TransparentMaterial.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

#include <algorithm>

using namespace std;
using namespace raytracer;

Colord TransparentMaterial::shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth) {
  Vector3d out = -ray.direction();
  Vector3d in;
  Colord reflectedColor = m_reflectiveBRDF.sample(hitPoint, out, in);
  Ray reflected(hitPoint.point(), in);
  
  if (m_specularBTDF.totalInternalReflection(ray, hitPoint)) {
    return raytracer->rayColor(reflected.epsilonShifted(), recursionDepth + 1);
  } else {
    auto color = PhongMaterial::shade(raytracer, ray, hitPoint, recursionDepth);
    
    Vector3d trans;
    Colord transmittedColor = m_specularBTDF.sample(hitPoint, out, trans);
    Ray transmitted(hitPoint.point(), trans);
    
    color += reflectedColor * raytracer->rayColor(reflected.epsilonShifted(), recursionDepth + 1) * fabs(hitPoint.normal() * in);
    color += transmittedColor * raytracer->rayColor(transmitted.epsilonShifted(), recursionDepth + 1) * fabs(hitPoint.normal() * trans);
    return color;
  }
}
