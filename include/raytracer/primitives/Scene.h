#pragma once

#include "raytracer/primitives/Composite.h"
#include "core/Color.h"

namespace raytracer {
  class Light;

  class Scene : public Composite {
  public:
    typedef std::list<Light*> Lights;

    inline Scene()
      : m_ambient(Colord::white()),
        m_background(Colord::white())
    {
    }

    inline Scene(const Colord& ambient)
      : m_ambient(ambient),
        m_background(Colord::white())
    {
    }

    ~Scene();

    inline void addLight(Light* light) {
      m_lights.push_back(light);
    }
    
    inline const Lights& lights() const {
      return m_lights;
    }
    
    inline const Colord& ambient() const {
      return m_ambient;
    }
    
    inline void setAmbient(const Colord& ambient) {
      m_ambient = ambient;
    }
    
    inline const Colord& background() const {
      return m_background;
    }
    
    inline void setBackground(const Colord& background) {
      m_background = background;
    }
    
  private:
    Lights m_lights;
    Colord m_ambient;
    Colord m_background;
  };
}
