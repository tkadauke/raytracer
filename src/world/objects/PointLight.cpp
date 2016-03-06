#include "world/objects/ElementFactory.h"
#include "world/objects/PointLight.h"
#include "raytracer/lights/PointLight.h"

PointLight::PointLight(Element* parent)
  : Light(parent)
{
}

raytracer::Light* PointLight::toRaytracer() const {
  return new raytracer::PointLight(globalTransform() * Vector3d::null(), color() * intensity());
}

static bool dummy = ElementFactory::self().registerClass<PointLight>("PointLight");

#include "PointLight.moc"
