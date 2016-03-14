#include "raytracer/materials/MatteMaterial.h"
#include "raytracer/State.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/lights/Light.h"
#include "core/math/Ray.h"
#include "raytracer/textures/Texture.h"

#include <algorithm>

using namespace std;
using namespace raytracer;

Colord MatteMaterial::shade(Raytracer* raytracer, const Rayd& ray, const HitPoint& hitPoint, State& state) {
  auto texColor = diffuseTexture() ? diffuseTexture()->evaluate(ray, hitPoint) : Colord::black();
  
  Lambertian ambientBRDF(texColor, ambientCoefficient());
  Lambertian diffuseBRDF(texColor, diffuseCoefficient());
  
  // for diffuse BRDFs the in and out vectors are irrelevant, so let's not calculate them
  auto color = ambientBRDF.reflectance(hitPoint, Vector3d::null()) * raytracer->scene()->ambient();

  for (const auto& light : raytracer->scene()->lights()) {
    Vector3d in = light->direction(hitPoint.point());
    
    if (raytracer->scene()->intersects(Rayd(hitPoint.point(), in).epsilonShifted(), state)) {
      state.shadowHit("MatteMaterial");
    } else {
      state.shadowMiss("MatteMaterial");
      double normalDotIn = hitPoint.normal() * in;
      if (normalDotIn > 0.0)
        color += diffuseBRDF(hitPoint, Vector3d::null(), Vector3d::null()) * light->radiance() * normalDotIn;
    }
  }
  
  return color;
}
