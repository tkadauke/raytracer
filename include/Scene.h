#ifndef SCENE_H
#define SCENE_H

#include "Composite.h"
#include "Color.h"

class Light;

class Scene : public Composite {
public:
  typedef std::list<Light*> Lights;
  
  Scene(const Colord& ambient)
    : m_ambient(ambient)
  {
  }
  
  ~Scene();
  
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);
  
  inline void addLight(Light* light) { m_lights.push_back(light); }
  const Lights& lights() const { return m_lights; }
  
  const Colord& ambient() const { return m_ambient; }

private:
  Lights m_lights;
  Colord m_ambient;
};

#endif
