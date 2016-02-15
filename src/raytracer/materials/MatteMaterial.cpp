#include "raytracer/materials/MatteMaterial.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/Light.h"
#include "core/math/Ray.h"

#include <algorithm>

using namespace std;
using namespace raytracer;

Colord MatteMaterial::shade(Raytracer* raytracer, const Ray&, const HitPoint& hitPoint, int) {
  // for diffuse BRDFs the in and out vectors are irrelevant, so let's not calculate them
  auto color = m_ambientBRDF.reflectance(hitPoint, Vector3d::null()) * raytracer->scene()->ambient();

  for (const auto& light : raytracer->scene()->lights()) {
    Vector3d lightDirection = (light->position() - hitPoint.point()).normalized();
    
    if (!raytracer->scene()->intersects(Ray(hitPoint.point(), lightDirection).epsilonShifted())) {
      double normalDotIn = hitPoint.normal() * lightDirection;
      if (normalDotIn > 0.0)
        color += m_diffuseBRDF(hitPoint, Vector3d::null(), Vector3d::null()) * light->color() * normalDotIn;
    }
  }
  
  return color;
}
