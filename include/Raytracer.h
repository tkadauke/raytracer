#ifndef RAYTRACER_H
#define RAYTRACER_H

#include "Color.h"
#include "math/Vector.h"

class Scene;
class Buffer;
class Ray;
class Camera;

class Raytracer {
public:
  Raytracer(Scene* scene);
  Raytracer(Camera* camera, Scene* scene);
  virtual ~Raytracer();
  
  void render(Buffer& buffer);
  
  Colord rayColor(const Ray& ray, int recursionDepth = 0);
  Camera* camera() const { return m_camera; }
  void setCamera(Camera* camera);
  
  void cancel();
  
  inline Scene* scene() const { return m_scene; }
  
private:
  Camera* m_camera;
  Scene* m_scene;
  
  class Private;
  Private* p;
};

#endif
