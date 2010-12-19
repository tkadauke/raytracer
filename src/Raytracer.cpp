#include "Raytracer.h"
#include "math/Vector.h"
#include "math/Ray.h"
#include "surfaces/Scene.h"
#include "Buffer.h"
#include "math/HitPoint.h"
#include "math/HitPointInterval.h"
#include "materials/Material.h"
#include "math/Matrix.h"
#include "cameras/Camera.h"
#include "core/Exception.h"

#include "cameras/PinholeCamera.h"

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
