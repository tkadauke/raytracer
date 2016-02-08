#pragma once

#include "raytracer/primitives/Composite.h"
#include "core/Color.h"

namespace raytracer {
  class Light;

  class Scene : public Composite {
  public:
    typedef std::list<Light*> Lights;

    Scene()
      : m_ambient(Colord::white())
    {
    }

    Scene(const Colord& ambient)
      : m_ambient(ambient)
    {
    }

    ~Scene();

    inline void addLight(Light* light) { m_lights.push_back(light); }
    const Lights& lights() const { return m_lights; }

    const Colord& ambient() const { return m_ambient; }
    inline void setAmbient(const Colord& ambient) { m_ambient = ambient; }

  private:
    Lights m_lights;
    Colord m_ambient;
  };
}
