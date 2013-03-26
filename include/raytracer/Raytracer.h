#ifndef RAYTRACER_H
#define RAYTRACER_H

#include "core/Color.h"
#include "core/math/Vector.h"

template<class T>
class Buffer;
class Ray;

namespace raytracer {
  class Scene;
  class Camera;
  class Primitive;

  class Raytracer {
  public:
    Raytracer(Scene* scene);
    Raytracer(Camera* camera, Scene* scene);
    virtual ~Raytracer();
  
    void render(Buffer<unsigned int>& buffer);
  
    Primitive* primitiveForRay(const Ray& ray);
    Colord rayColor(const Ray& ray, int recursionDepth = 0);
    Camera* camera() const { return m_camera; }
    void setCamera(Camera* camera);
  
    void cancel();
    void uncancel();
  
    inline Scene* scene() const { return m_scene; }
    inline void setScene(Scene* scene) { m_scene = scene; }
  
  private:
    Camera* m_camera;
    Scene* m_scene;
  
    class Private;
    Private* p;
  };
}

#endif
