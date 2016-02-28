#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"

template<class T>
class Buffer;
class Ray;

namespace raytracer {
  class Scene;
  class Camera;
  class Primitive;

  class Raytracer : public std::enable_shared_from_this<Raytracer> {
  public:
    Raytracer(Scene* scene);
    Raytracer(std::shared_ptr<Camera> camera, Scene* scene);
    virtual ~Raytracer();
  
    void render(Buffer<unsigned int>& buffer);
  
    Primitive* primitiveForRay(const Ray& ray);
    Colord rayColor(const Ray& ray, int recursionDepth = 0);
    
    inline std::shared_ptr<Camera> camera() const {
      return m_camera;
    }
    
    inline void setCamera(std::shared_ptr<Camera> camera) {
      m_camera = camera;
    }
    
    void cancel();
    void uncancel();
  
    inline Scene* scene() const {
      return m_scene;
    }
    
    inline void setScene(Scene* scene) {
      m_scene = scene;
    }
    
    void setMaximumRecursionDepth(int depth);

  private:
    std::shared_ptr<Camera> m_camera;
    Scene* m_scene;
  
    struct Private;
    std::unique_ptr<Private> p;
  };
}
