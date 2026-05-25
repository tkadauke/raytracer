#include "world/objects/Surface.h"
#include "world/objects/Material.h"
#include "world/objects/Group.h"
#include "world/objects/Light.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Grid.h"

Surface::Surface(Element* parent)
    : Transformable(parent),
      m_material(nullptr),
      m_visible(true),
      m_velocity(Vector3d::null) {
}

std::shared_ptr<render::Primitive>
Surface::applyTransform(std::shared_ptr<render::Primitive> primitive) const {
  auto result = std::make_shared<render::Instance>(primitive);
  result->setMatrix(localTransform());
  result->setVelocity(m_velocity);
  return result;
}

std::shared_ptr<render::Primitive> Surface::toRaytracer(render::Scene* scene) const {
  auto primitive = toRaytracerPrimitive();
  if (!primitive) {
    return primitive;
  }

  if (material()) {
    primitive->setMaterial(material()->toRaytracerMaterial());
  }

  if (childElements().size() > 0) {
    auto composite = std::dynamic_pointer_cast<render::Composite>(primitive);
    if (!composite) {
      composite = std::make_shared<render::Composite>();
      composite->add(primitive);
    }

    for (const auto& child : childElements()) {
      if (Surface* surface = qobject_cast<Surface*>(child)) {
        if (surface->visible())
          composite->add(surface->toRaytracer(scene));
      } else if (Group* group = qobject_cast<Group*>(child)) {
        if (group->visible()) {
          auto primitive = group->toRaytracer(scene);
          if (primitive)
            composite->add(primitive);
        }
      } else if (Light* light = qobject_cast<Light*>(child)) {
        if (light->visible())
          scene->addLight(light->toRaytracer());
      }
    }

    if (auto grid = std::dynamic_pointer_cast<render::Grid>(composite)) {
      grid->setup();
    }

    return applyTransform(composite);
  } else {
    return applyTransform(primitive);
  }
}

bool Surface::canHaveChild(Element* child) const {
  return dynamic_cast<Surface*>(child) != nullptr || dynamic_cast<Light*>(child) != nullptr ||
         dynamic_cast<Group*>(child) != nullptr;
}
