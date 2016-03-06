#include "world/objects/ElementFactory.h"
#include "world/objects/DirectionalLight.h"
#include "raytracer/lights/DirectionalLight.h"

DirectionalLight::DirectionalLight(Element* parent)
  : Light(parent),
    m_direction(-0.5, -1, -0.5)
{
}

raytracer::Light* DirectionalLight::toRaytracer() const {
  return new raytracer::DirectionalLight(Matrix3d(globalTransform()) * direction(), color() * intensity());
}

static bool dummy = ElementFactory::self().registerClass<DirectionalLight>("DirectionalLight");

#include "DirectionalLight.moc"
