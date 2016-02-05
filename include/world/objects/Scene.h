#ifndef SCENE_H
#define SCENE_H

#include "world/objects/Element.h"

namespace raytracer {
  class Scene;
}

class Scene : public Element {
  Q_OBJECT
  
public:
  Scene(Element* parent);
  
  raytracer::Scene* toRaytracerScene() const;
};

#endif
