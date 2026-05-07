#include "world/objects/ElementFactory.h"
#include "world/objects/PointLight.h"
#include "render/lights/PointLight.h"

PointLight::PointLight(Element* parent)
  : Light(parent)
{
}

std::shared_ptr<render::Light> PointLight::toRaytracer() const {
  return make_named<render::PointLight>(globalTransform() * Vector3d::null(), color() * intensity());
}

static bool dummy = ElementFactory::self().registerClass<PointLight>("PointLight");

