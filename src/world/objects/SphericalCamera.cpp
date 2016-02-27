#include "world/objects/ElementFactory.h"
#include "world/objects/SphericalCamera.h"

#include "raytracer/cameras/SphericalCamera.h"

SphericalCamera::SphericalCamera(Element* parent)
  : Camera(parent), m_horizontalFieldOfView(Angled::fromDegrees(180)), m_verticalFieldOfView(Angled::fromDegrees(120))
{
}

std::shared_ptr<raytracer::Camera> SphericalCamera::toRaytracer() const {
  auto camera = std::make_shared<raytracer::SphericalCamera>(position(), target());
  camera->setFieldOfView(
    horizontalFieldOfView(),
    verticalFieldOfView()
  );
  return camera;
}

static bool dummy = ElementFactory::self().registerClass<SphericalCamera>("SphericalCamera");

#include "SphericalCamera.moc"

