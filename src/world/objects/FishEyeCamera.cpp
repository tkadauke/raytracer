#include "world/objects/ElementFactory.h"
#include "world/objects/FishEyeCamera.h"

#include "render/cameras/FishEyeCamera.h"

FishEyeCamera::FishEyeCamera(Element* parent)
    : Camera(parent),
      m_fieldOfView(180_degrees) {
}

std::shared_ptr<render::Camera> FishEyeCamera::toRaytracer() const {
  auto camera = make_named<render::FishEyeCamera>(position(), target());
  camera->setFieldOfView(fieldOfView());
  applyCameraProperties(camera);
  return camera;
}

static bool dummy = ElementFactory::self().registerClass<FishEyeCamera>("FishEyeCamera");
