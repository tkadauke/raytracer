#include "world/objects/ElementFactory.h"
#include "world/objects/TiltShiftCamera.h"

#include "raytracer/cameras/TiltShiftCamera.h"

TiltShiftCamera::TiltShiftCamera(Element* parent)
  : ThinLensCamera(parent),
    m_tilt(0_degrees),
    m_shiftX(0),
    m_shiftY(0)
{
}

std::shared_ptr<raytracer::Camera> TiltShiftCamera::toRaytracer() const {
  auto camera = make_named<raytracer::TiltShiftCamera>(position(), target());
  camera->setDistance(distance());
  camera->setZoom(zoom());
  camera->setApertureRadius(apertureRadius());
  camera->setFocalDistance(focalDistance());
  camera->setTilt(m_tilt);
  camera->setShift(Vector2d(m_shiftX, m_shiftY));
  return camera;
}

static bool dummy = ElementFactory::self().registerClass<TiltShiftCamera>("TiltShiftCamera");
