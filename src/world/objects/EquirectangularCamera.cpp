#include "world/objects/ElementFactory.h"
#include "world/objects/EquirectangularCamera.h"

#include "render/cameras/EquirectangularCamera.h"

EquirectangularCamera::EquirectangularCamera(Element* parent)
    : Camera(parent) {
}

std::shared_ptr<render::Camera> EquirectangularCamera::toRaytracer() const {
  auto camera = make_named<render::EquirectangularCamera>(position(), target());
  applyCameraProperties(camera);
  return camera;
}

static bool dummy =
  ElementFactory::self().registerClass<EquirectangularCamera>("EquirectangularCamera");
