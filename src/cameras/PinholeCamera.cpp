#include "cameras/PinholeCamera.h"
#include "Buffer.h"
#include "Raytracer.h"
#include "math/Ray.h"
#include "viewplanes/ViewPlane.h"

void PinholeCamera::render(Raytracer* raytracer, Buffer& buffer) {
  Matrix4d m = matrix();
  ViewPlane* plane = viewPlane();
  plane->setup(m, buffer.width(), buffer.height());

  Vector3d position = m * Vector3d(0, 0, -m_distance);

  for (ViewPlane::Iterator pixel = plane->begin(), end = plane->end(); pixel != end; ++pixel) {
    Ray ray(position, (*pixel - position).normalized());
    plot(buffer, pixel, raytracer->rayColor(ray));
    
    if (isCancelled())
      break;
  }

  uncancel();
}
