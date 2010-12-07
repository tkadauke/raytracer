#ifndef RAYTRACER_H
#define RAYTRACER_H

#include "Color.h"
#include "Vector.h"

class Scene;
class Buffer;
class Ray;
class Camera;

class Raytracer {
public:
  Raytracer(Scene* scene);
  Raytracer(Camera* camera, Scene* scene)
    : m_camera(camera), m_scene(scene) {}
  
  void render(Buffer& buffer);
  
  Colord rayColor(const Ray& ray, int recursionDepth = 0);
  Camera* camera() const { return m_camera; }
  
private:
  Vector3d refract(const Vector3d& direction, const Vector3d& normal, double outerRefractionIndex, double innerRefractionIndex);
  
  Camera* m_camera;
  Scene* m_scene;
};

#endif
