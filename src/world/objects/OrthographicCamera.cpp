#include "world/objects/ElementFactory.h"
#include "world/objects/OrthographicCamera.h"

#include "raytracer/cameras/OrthographicCamera.h"

OrthographicCamera::OrthographicCamera(Element* parent)
  : Camera(parent),
    m_zoom(1)
{
}

std::shared_ptr<raytracer::Camera> OrthographicCamera::toRaytracer() const {
  auto camera = std::make_shared<raytracer::OrthographicCamera>(position(), target());
  camera->setZoom(zoom());
  return camera;
}

static bool dummy = ElementFactory::self().registerClass<OrthographicCamera>("OrthographicCamera");

#include "OrthographicCamera.moc"
