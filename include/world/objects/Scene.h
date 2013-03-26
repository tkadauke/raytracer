#ifndef SCENE_H
#define SCENE_H

#include "world/objects/Object.h"

namespace raytracer {
  class Scene;
}

class Surface;

class Scene : public Object {
public:
  typedef std::list<Surface*> Children;
  
  Scene();
  
  raytracer::Scene* toRaytracerScene() const;
  
  inline void add(Surface* child) { m_children.push_back(child); }
  inline const Children& children() const { return m_children; }
  
private:
  Children m_children;
};

#endif
