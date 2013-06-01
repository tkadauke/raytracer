#include "world/objects/Scene.h"
#include "world/objects/Surface.h"
#include "raytracer/primitives/Scene.h"

Scene::Scene() {
}

raytracer::Scene* Scene::toRaytracerScene() const {
  raytracer::Scene* result = new raytracer::Scene;
  
  for (Children::const_iterator i = children().begin(); i != children().end(); ++i) {
    Surface* surface = dynamic_cast<Surface*>(*i);
    if (surface && surface->visible()) {
      raytracer::Primitive* primitive = surface->toRaytracerPrimitive();
      result->add(primitive);
    }
  }
  
  result->setAmbient(Colord(0.4, 0.4, 0.4));
  
  return result;
}
