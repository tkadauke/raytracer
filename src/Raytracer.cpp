#include "Raytracer.h"
#include "Vector.h"
#include "Ray.h"
#include "Scene.h"
#include "Buffer.h"
#include "HitPoint.h"
#include "HitPointInterval.h"
#include "Material.h"
#include "Matrix.h"
#include "Camera.h"
#include "Exception.h"

#include "PinholeCamera.h"

Raytracer::Raytracer(Scene* scene)
  : m_scene(scene)
{
  m_camera = new PinholeCamera;
}

void Raytracer::render(Buffer& buffer) {
  m_camera->render(this, buffer);
}

Colord Raytracer::rayColor(const Ray& ray, int recursionDepth) {
  if (recursionDepth == 5) {
    return m_scene->ambient();
  }
  
  HitPointInterval hitPoints;
  
  Surface* surface = m_scene->intersect(ray, hitPoints);
  if (surface) {
    HitPoint hitPoint = hitPoints.minWithPositiveDistance();
    
    Colord color = surface->material()->shade(this, ray, hitPoint, recursionDepth);
    
    return color;
  } else {
    return m_scene->ambient();
  }
}

void Raytracer::setCamera(Camera* camera) {
  if (m_camera)
    delete m_camera;
  m_camera = camera;
}

void Raytracer::cancel() {
  m_camera->cancel();
}
