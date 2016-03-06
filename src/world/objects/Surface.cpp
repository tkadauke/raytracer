#include "world/objects/Surface.h"
#include "world/objects/Material.h"
#include "world/objects/Light.h"
#include "raytracer/primitives/Instance.h"
#include "raytracer/primitives/Composite.h"
#include "raytracer/primitives/Scene.h"

Surface::Surface(Element* parent)
  : Transformable(parent),
    m_material(nullptr),
    m_visible(true)
{
}

std::shared_ptr<raytracer::Primitive> Surface::applyTransform(std::shared_ptr<raytracer::Primitive> primitive) const {
  auto result = std::make_shared<raytracer::Instance>(primitive);
  result->setMatrix(localTransform());
  return result;
}

std::shared_ptr<raytracer::Primitive> Surface::toRaytracer(raytracer::Scene* scene) const {
  auto primitive = toRaytracerPrimitive();
  if (!primitive) {
    return primitive;
  }
  
  if (material()) {
    primitive->setMaterial(material()->toRaytracerMaterial());
  } else {
    primitive->setMaterial(Material::defaultMaterial()->toRaytracerMaterial());
  }
  
  if (childElements().size() > 0) {
    auto composite = std::dynamic_pointer_cast<raytracer::Composite>(primitive);
    if (!composite) {
      composite = std::make_shared<raytracer::Composite>();
      composite->add(primitive);
    }
    
    for (const auto& child : childElements()) {
      if (Surface* surface = qobject_cast<Surface*>(child)) {
        if (surface->visible())
          composite->add(surface->toRaytracer(scene));
      } else if (Light* light = qobject_cast<Light*>(child)) {
        if (light->visible())
          scene->addLight(light->toRaytracer());
      }
    }

    return applyTransform(composite);
  } else {
    return applyTransform(primitive);
  }
}

bool Surface::canHaveChild(Element* child) const {
  return dynamic_cast<Surface*>(child) != nullptr ||
         dynamic_cast<Light*>(child) != nullptr;
}

#include "Surface.moc"
