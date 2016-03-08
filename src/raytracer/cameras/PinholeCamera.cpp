#include "raytracer/cameras/CameraFactory.h"
#include "raytracer/cameras/PinholeCamera.h"
#include "core/math/Ray.h"
#include "raytracer/viewplanes/ViewPlane.h"

using namespace raytracer;

void PinholeCamera::render(std::shared_ptr<Raytracer> raytracer, Buffer<unsigned int>& buffer, const Recti& rect) {
  viewPlane()->setPixelSize(1.0 / m_zoom);
  Camera::render(raytracer, buffer, rect);
}

Rayd PinholeCamera::rayForPixel(double x, double y) {
  Vector3d position = matrix() * Vector4d(0, 0, -m_distance);
  Vector3d pixel = viewPlane()->pixelAt(x, y);
  return Rayd(position, (pixel - position).normalized());
}

static bool dummy = CameraFactory::self().registerClass<PinholeCamera>("PinholeCamera");
