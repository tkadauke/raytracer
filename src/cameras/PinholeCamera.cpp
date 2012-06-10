#include "cameras/CameraFactory.h"
#include "cameras/PinholeCamera.h"
#include "Buffer.h"
#include "Raytracer.h"
#include "math/Ray.h"
#include "viewplanes/ViewPlane.h"

void PinholeCamera::render(Raytracer* raytracer, Buffer<unsigned int>& buffer, const Rect& rect) {
  Matrix4d m = matrix();
  ViewPlane* plane = viewPlane();
  plane->setPixelSize(1.0 / m_zoom);

  Vector3d position = m * Vector4d(0, 0, -m_distance);

  for (ViewPlane::Iterator pixel = plane->begin(rect), end = plane->end(rect); pixel != end; ++pixel) {
    Ray ray(position, (*pixel - position).normalized());
    plot(buffer, rect, pixel, raytracer->rayColor(ray));
    
    if (isCancelled())
      break;
  }
}

static bool dummy = CameraFactory::self().registerClass<PinholeCamera>("PinholeCamera");
