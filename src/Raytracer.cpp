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
#include "Exception.h"

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
  if (recursionDepth == 5) {
    return m_scene->ambient();
  }
  
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
                 light->color() * highlightColor * pow(h * hitPoint.normal(), 128);
      }
    }
    
    if (surface->material().isSpecular()) {
      Vector3d reflectionDirection = ray.direction() - hitPoint.normal() * (2.0 * (ray.direction() * hitPoint.normal()));

      if (surface->material().isRefractive()) {
        double refractionIndex = surface->material().refractionIndex();
        Vector3d refractionDirection;
        double angle;
        Colord transparency;
        
        if (ray.direction() * hitPoint.normal() < 0) {
          refractionDirection = refract(ray.direction(), hitPoint.normal(), 1.0, refractionIndex);
          angle = -ray.direction() * hitPoint.normal();
          transparency = Colord::white;
        } else {
          double distance = hitPoint.distance();
          Colord absorbance = surface->material().absorbanceColor() * -distance * 0.15;
          transparency = Colord(expf(absorbance.r()), expf(absorbance.g()), expf(absorbance.b()));
          
          refractionDirection = refract(ray.direction(), -hitPoint.normal(), refractionIndex, 1.0);
          
          if (refractionDirection.isUndefined()) {
            return color + rayColor(Ray(hitPoint.point(), reflectionDirection).epsilonShifted(), recursionDepth + 1) * transparency;
          } else {
            angle = refractionDirection * hitPoint.normal();
          }
        }
        
        double R0 = (refractionIndex - 1.0) * (refractionIndex - 1.0) / (refractionIndex + 1.0) * (refractionIndex + 1.0);
        double R = R0 + (1.0 - R0) * pow(1.0 - angle, 5.0);
        
        color = color + transparency * (rayColor(Ray(hitPoint.point(), reflectionDirection).epsilonShifted(), recursionDepth + 1) * R +
                                        rayColor(Ray(hitPoint.point(), refractionDirection).epsilonShifted(), recursionDepth + 1) * (1.0 - R));
      } else if (surface->material().isReflective()) {
        color = color + specularColor * rayColor(Ray(hitPoint.point(), reflectionDirection).epsilonShifted(), recursionDepth + 1);
      }
    }
    
    return color;
  } else {
    return m_scene->ambient();
  }
}

Vector3d Raytracer::refract(const Vector3d& direction, const Vector3d& normal, double outerRefractionIndex, double innerRefractionIndex) {
  double cosTheta = direction * normal;
  double refractionIndex = outerRefractionIndex / innerRefractionIndex;
  double cosPhiSquared = 1.0 - (refractionIndex * refractionIndex * (1.0 - cosTheta * cosTheta));
  
  if (cosPhiSquared < 0.0) {
    return Vector3d::undefined;
  } else {
    return (direction * refractionIndex) + normal * (refractionIndex * -cosTheta - sqrt(cosPhiSquared));
  }
}
