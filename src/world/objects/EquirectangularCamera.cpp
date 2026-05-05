#include "world/objects/ElementFactory.h"
#include "world/objects/EquirectangularCamera.h"

#include "raytracer/cameras/EquirectangularCamera.h"

EquirectangularCamera::EquirectangularCamera(Element* parent)
  : Camera(parent)
{
}

std::shared_ptr<raytracer::Camera> EquirectangularCamera::toRaytracer() const {
  return make_named<raytracer::EquirectangularCamera>(position(), target());
}

static bool dummy = ElementFactory::self().registerClass<EquirectangularCamera>("EquirectangularCamera");
