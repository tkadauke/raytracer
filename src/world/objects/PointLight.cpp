#include "world/objects/ElementFactory.h"
#include "world/objects/PointLight.h"
#include "raytracer/lights/PointLight.h"

PointLight::PointLight(Element* parent)
  : Light(parent)
{
}

std::shared_ptr<raytracer::Light> PointLight::toRaytracer() const {
  return make_named<raytracer::PointLight>(globalTransform() * Vector3d::null(), color() * intensity());
}

static bool dummy = ElementFactory::self().registerClass<PointLight>("PointLight");

#include "PointLight.moc"
