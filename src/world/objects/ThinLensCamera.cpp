#include "world/objects/ElementFactory.h"
#include "world/objects/ThinLensCamera.h"

#include "render/cameras/ThinLensCamera.h"

ThinLensCamera::ThinLensCamera(Element* parent)
  : Camera(parent),
    m_distance(5),
    m_zoom(1),
    m_apertureRadius(0.1),
    m_focalDistance(5)
{
}

std::shared_ptr<render::Camera> ThinLensCamera::toRaytracer() const {
  auto camera = make_named<render::ThinLensCamera>(position(), target());
  camera->setDistance(m_distance);
  camera->setZoom(m_zoom);
  camera->setApertureRadius(m_apertureRadius);
  camera->setFocalDistance(m_focalDistance);
  return camera;
}

static bool dummy = ElementFactory::self().registerClass<ThinLensCamera>("ThinLensCamera");
