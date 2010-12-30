#ifndef SCENE_H
#define SCENE_H

#include "surfaces/Composite.h"
#include "Color.h"

class Light;

class Scene : public Composite {
public:
  typedef std::list<Light*> Lights;
  
  Scene()
    : m_ambient(Colord::white)
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

private:
  Lights m_lights;
  Colord m_ambient;
};

#endif
