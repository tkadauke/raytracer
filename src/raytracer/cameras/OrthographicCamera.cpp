#include "raytracer/cameras/CameraFactory.h"
#include "raytracer/cameras/OrthographicCamera.h"
#include "core/math/Ray.h"
#include "raytracer/viewplanes/ViewPlane.h"

using namespace raytracer;

void OrthographicCamera::render(std::shared_ptr<Raytracer> raytracer, Buffer<unsigned int>& buffer, const Recti& rect) {
  viewPlane()->setPixelSize(1.0 / m_zoom);
  Camera::render(raytracer, buffer, rect);
}

Ray OrthographicCamera::rayForPixel(double x, double y) {
  Vector3d direction = Matrix3d(matrix()) * Vector3d::forward();
  Vector3d pixel = viewPlane()->pixelAt(x, y);
  return Ray(pixel, direction);
}

static bool dummy = CameraFactory::self().registerClass<OrthographicCamera>("OrthographicCamera");
