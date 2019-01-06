#include "world/objects/ElementFactory.h"
#include "world/objects/SphericalCamera.h"

#include "raytracer/cameras/SphericalCamera.h"

SphericalCamera::SphericalCamera(Element* parent)
  : Camera(parent),
    m_horizontalFieldOfView(180_degrees),
    m_verticalFieldOfView(120_degrees)
{
}

std::shared_ptr<raytracer::Camera> SphericalCamera::toRaytracer() const {
  auto camera = make_named<raytracer::SphericalCamera>(position(), target());
  camera->setFieldOfView(
    horizontalFieldOfView(),
    verticalFieldOfView()
  );
  return camera;
}

static bool dummy = ElementFactory::self().registerClass<SphericalCamera>("SphericalCamera");

#include "SphericalCamera.moc"

