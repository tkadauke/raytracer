#include "world/objects/Scene.h"
#include "world/objects/Surface.h"
#include "raytracer/primitives/Scene.h"

Scene::Scene(Element* parent)
  : Element(parent)
{
}

raytracer::Scene* Scene::toRaytracerScene() const {
  raytracer::Scene* result = new raytracer::Scene;
  
  for (const auto& child : children()) {
    auto surface = dynamic_cast<Surface*>(child);
    if (surface && surface->visible()) {
       auto primitive = surface->toRaytracerPrimitive();
      result->add(primitive);
    }
  }
  
  result->setAmbient(Colord(0.4, 0.4, 0.4));
  
  return result;
}

#include "Scene.moc"
