#include "Raytracer.h"
#include "LinearInterpolation.h"
#include "Vector.h"
#include "Ray.h"
#include "Scene.h"
#include "Buffer.h"
#include "HitPoint.h"
#include "HitPointInterval.h"
#include "Light.h"
#include "Matrix.h"
#include "Camera.h"

#include <algorithm>

using namespace std;

void Raytracer::render(Camera camera, Buffer& buffer) {
  Matrix4d matrix = camera.matrix();
  
  Vector3d position = matrix * Vector3d(0, 0, -5);
  Vector3d topLeft = matrix * Vector3d(-4, -3, 0),
           topRight = matrix * Vector3d(4, -3, 0),
           bottomLeft = matrix * Vector3d(-4, 3, 0),
           bottomRight = matrix * Vector3d(4, 3, 0);

  LinearInterpolation<Vector3d> leftEnd(topLeft, bottomLeft, buffer.height()), rightEnd(topRight, bottomRight, buffer.height());
  
  for (LinearInterpolation<Vector3d>::Iterator left = leftEnd.begin(), right = rightEnd.begin(); left != leftEnd.end(); ++left, ++right) {
    
    LinearInterpolation<Vector3d> row(*left, *right, buffer.width());
    for (LinearInterpolation<Vector3d>::Iterator pixel = row.begin(); pixel != row.end(); ++pixel) {
      Ray ray(position, (*pixel - position).normalized());
      buffer[left.current()][pixel.current()] = rayColor(ray);
    }
  }
}

Colord Raytracer::rayColor(const Ray& ray, int recursionDepth) {
  HitPointInterval hitPoints;
  
  Surface* surface = m_scene->intersect(ray, hitPoints);
  if (surface) {
    HitPoint hitPoint = hitPoints.minWithPositiveDistance();
    const Colord& diffuseColor = surface->material().diffuseColor();
    const Colord& highlightColor = surface->material().highlightColor();
    const Colord& specularColor = surface->material().specularColor();
    
    Colord color = m_scene->ambient() * diffuseColor;
    
    for (Scene::Lights::const_iterator l = m_scene->lights().begin(); l != m_scene->lights().end(); ++l) {
      Light* light = *l;
      
      HitPointInterval lightHitPoint;
      Vector3d lightDirection = (light->position() - hitPoint.point()).normalized();
      
      if (!m_scene->intersect(Ray(hitPoint.point(), lightDirection).epsilonShifted(), lightHitPoint)) {
        Vector3d h = (lightDirection - ray.direction()).normalized();
        color = color + diffuseColor * light->color() * max(0.0, hitPoint.normal() * lightDirection) +
                 light->color() * highlightColor * std::pow(h * hitPoint.normal(), 128);
      }
    }
    
    if (recursionDepth < 5) {
      if (surface->material().isReflective()) {
        Vector3d reflectionDirection = ray.direction() - hitPoint.normal() * (2 * (ray.direction() * hitPoint.normal()));
        color = color + specularColor * rayColor(Ray(hitPoint.point(), reflectionDirection).epsilonShifted(), recursionDepth + 1);
      }
      
      if (surface->material().isRefractive()) {
        double refractionIndex = surface->material().refractionIndex();
        Vector3d refractionDirection;
        
        if (ray.direction() * hitPoint.normal() < 0) {
          refractionDirection = refract(ray.direction(), hitPoint.normal(), 1.0, refractionIndex);
          color = color + rayColor(Ray(hitPoint.point(), refractionDirection).epsilonShifted(), recursionDepth + 1);
        } else {
          refractionDirection = refract(ray.direction(), -hitPoint.normal(), refractionIndex, 1.0);
          if (refractionDirection != Vector3d::undefined) {
            double distance = (hitPoint.point() - ray.origin()).length();
            Colord absorbance = surface->material().absorbanceColor() * -distance * 0.15;
            Colord transparency = Colord(expf(absorbance.r()), expf(absorbance.g()), expf(absorbance.b()));
            
            color = color + transparency * rayColor(Ray(hitPoint.point(), refractionDirection).epsilonShifted(), recursionDepth + 1);
          }
        }
      }
    }
    
    return color;
  } else {
    return Colord(0.5, 0.5, 0.5);
  }
}

Vector3d Raytracer::refract(const Vector3d& direction, const Vector3d& normal, double outerRefractionIndex, double innerRefractionIndex) {
  double cosTheta = direction * normal;
  double refractionIndex = outerRefractionIndex / innerRefractionIndex;
  double cosPhiSquared = 1 - (refractionIndex * refractionIndex * (1 - cosTheta * cosTheta));
  
  if (cosPhiSquared < 0.0) {
    return Vector3d::undefined;
  } else {
    return (direction * refractionIndex) + normal * (refractionIndex * -cosTheta - sqrt(cosPhiSquared));
  }
}
