#include "world/objects/ElementFactory.h"
#include "world/objects/FishEyeCamera.h"

#include "raytracer/cameras/FishEyeCamera.h"

FishEyeCamera::FishEyeCamera(Element* parent)
  : Camera(parent),
    m_fieldOfView(Angled::fromDegrees(180))
{
}

std::shared_ptr<raytracer::Camera> FishEyeCamera::toRaytracer() const {
  auto camera = std::make_shared<raytracer::FishEyeCamera>(position(), target());
  camera->setFieldOfView(fieldOfView());
  return camera;
}

static bool dummy = ElementFactory::self().registerClass<FishEyeCamera>("FishEyeCamera");

#include "FishEyeCamera.moc"
