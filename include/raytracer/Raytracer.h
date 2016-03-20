#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"
#include "core/math/Ray.h"

template<class T>
class Buffer;

namespace raytracer {
  class Scene;
  class Camera;
  class Primitive;
  class State;

  class Raytracer : public std::enable_shared_from_this<Raytracer> {
  public:
    explicit Raytracer(Scene* scene);
    explicit Raytracer(std::shared_ptr<Camera> camera, Scene* scene);
    virtual ~Raytracer();
  
    void render(Buffer<unsigned int>& buffer);
  
    const Primitive* primitiveForRay(const Rayd& ray) const;
    State rayState(const Rayd& ray) const;
    Colord rayColor(const Rayd& ray, State& state) const;
    
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
