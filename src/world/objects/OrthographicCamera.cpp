#include "world/objects/ElementFactory.h"
#include "world/objects/OrthographicCamera.h"

#include "raytracer/cameras/OrthographicCamera.h"

OrthographicCamera::OrthographicCamera(Element* parent)
  : Camera(parent)
{
}

std::shared_ptr<raytracer::Camera> OrthographicCamera::toRaytracer() const {
  return std::make_shared<raytracer::OrthographicCamera>(position(), target());
}

static bool dummy = ElementFactory::self().registerClass<OrthographicCamera>("OrthographicCamera");

#include "OrthographicCamera.moc"
