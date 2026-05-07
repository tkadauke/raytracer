#include "render/cameras/CameraFactory.h"
#include "render/cameras/OrthographicCamera.h"
#include "core/math/Ray.h"
#include "render/viewplanes/ViewPlane.h"

using namespace render;

Rayd OrthographicCamera::rayForPixel(double x, double y, render::SampleStream&) const {
  Vector3d direction = Matrix3d(matrix()) * Vector3d::forward();
  Vector3d pixel = viewPlane()->pixelAt(x, y);
  return Rayd(pixel, direction);
}

void OrthographicCamera::setViewPlane(std::shared_ptr<render::ViewPlane> plane) {
  Camera::setViewPlane(plane);
  viewPlane()->setPixelSize(1.0 / m_zoom);
}

static bool dummy = CameraFactory::self().registerClass<OrthographicCamera>("OrthographicCamera");
