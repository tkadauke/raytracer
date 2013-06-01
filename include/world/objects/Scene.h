#ifndef SCENE_H
#define SCENE_H

#include "world/objects/Element.h"

namespace raytracer {
  class Scene;
}

class Scene : public Element {
public:
  Scene();
  
  raytracer::Scene* toRaytracerScene() const;
};

#endif
