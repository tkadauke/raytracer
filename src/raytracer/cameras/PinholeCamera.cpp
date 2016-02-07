#include "raytracer/cameras/CameraFactory.h"
#include "raytracer/cameras/PinholeCamera.h"
#include "core/Buffer.h"
#include "raytracer/Raytracer.h"
#include "core/math/Ray.h"
#include "raytracer/viewplanes/ViewPlane.h"

using namespace raytracer;

void PinholeCamera::render(Raytracer* raytracer, Buffer<unsigned int>& buffer, const Rect& rect) {
  auto plane = viewPlane();
  plane->setPixelSize(1.0 / m_zoom);

  Vector3d position = matrix() * Vector4d(0, 0, -m_distance);

  for (ViewPlane::Iterator pixel = plane->begin(rect), end = plane->end(rect); pixel != end; ++pixel) {
    Ray ray(position, (*pixel - position).normalized());
    plot(buffer, rect, pixel, raytracer->rayColor(ray));
    
    if (isCancelled())
      break;
  }
}

Ray PinholeCamera::rayForPixel(int x, int y) {
  Vector3d position = matrix() * Vector4d(0, 0, -m_distance);
  Vector3d pixel = viewPlane()->pixelAt(x, y);
  return Ray(position, (pixel - position).normalized());
}

static bool dummy = CameraFactory::self().registerClass<PinholeCamera>("PinholeCamera");
