#include "Material.h"
#include "Raytracer.h"
#include "HitPoint.h"
#include "Scene.h"
#include "Light.h"
#include "Ray.h"

#include <algorithm>

using namespace std;

Colord Material::shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth) {
  Colord color = raytracer->scene()->ambient() * m_diffuseColor;

  for (Scene::Lights::const_iterator l = raytracer->scene()->lights().begin(); l != raytracer->scene()->lights().end(); ++l) {
    Light* light = *l;
    
    Vector3d lightDirection = (light->position() - hitPoint.point()).normalized();
    
    if (!raytracer->scene()->intersects(Ray(hitPoint.point(), lightDirection).epsilonShifted())) {
      Vector3d h = (lightDirection - ray.direction()).normalized();
      color = color + m_diffuseColor * light->color() * max(0.0, hitPoint.normal() * lightDirection) +
               light->color() * m_highlightColor * pow(h * hitPoint.normal(), 128);
    }
  }
  
  if (isSpecular()) {
    Vector3d reflectionDirection = ray.direction() - hitPoint.normal() * (2.0 * (ray.direction() * hitPoint.normal()));

    if (isRefractive()) {
      Vector3d refractionDirection;
      double angle;
      Colord transparency;
      
      if (ray.direction() * hitPoint.normal() < 0) {
        refractionDirection = refract(ray.direction(), hitPoint.normal(), 1.0, m_refractionIndex);
        angle = -ray.direction() * hitPoint.normal();
        transparency = Colord::white;
      } else {
        double distance = hitPoint.distance();
        Colord absorbance = m_absorbanceColor * -distance * 0.15;
        transparency = Colord(expf(absorbance.r()), expf(absorbance.g()), expf(absorbance.b()));
        
        refractionDirection = refract(ray.direction(), -hitPoint.normal(), m_refractionIndex, 1.0);
        
        if (refractionDirection.isUndefined()) {
          return color + raytracer->rayColor(Ray(hitPoint.point(), reflectionDirection).epsilonShifted(), recursionDepth + 1) * transparency;
        } else {
          angle = refractionDirection * hitPoint.normal();
        }
      }
      
      double R0 = (m_refractionIndex - 1.0) * (m_refractionIndex - 1.0) / (m_refractionIndex + 1.0) * (m_refractionIndex + 1.0);
      double R = R0 + (1.0 - R0) * pow(1.0 - angle, 5.0);
      
      color = color + transparency * (raytracer->rayColor(Ray(hitPoint.point(), reflectionDirection).epsilonShifted(), recursionDepth + 1) * R +
                                      raytracer->rayColor(Ray(hitPoint.point(), refractionDirection).epsilonShifted(), recursionDepth + 1) * (1.0 - R));
    } else if (isReflective()) {
      color = color + m_specularColor * raytracer->rayColor(Ray(hitPoint.point(), reflectionDirection).epsilonShifted(), recursionDepth + 1);
    }
  }
  
  return color;
}

Vector3d Material::refract(const Vector3d& direction, const Vector3d& normal, double outerRefractionIndex, double innerRefractionIndex) {
  double cosTheta = direction * normal;
  double refractionIndex = outerRefractionIndex / innerRefractionIndex;
  double cosPhiSquared = 1.0 - (refractionIndex * refractionIndex * (1.0 - cosTheta * cosTheta));
  
  if (cosPhiSquared < 0.0) {
    return Vector3d::undefined;
  } else {
    return (direction * refractionIndex) + normal * (refractionIndex * -cosTheta - sqrt(cosPhiSquared));
  }
}
