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
  Raytracer(Scene* scene)
    : m_scene(scene) {}
  
  void render(Camera camera, Buffer& buffer);
  
protected:
  Colord rayColor(const Ray& ray, int recursionDepth = 0);
  
private:
  Vector3d refract(const Vector3d& direction, const Vector3d& normal, double outerRefractionIndex, double innerRefractionIndex);
  
  Scene* m_scene;
};

#endif
