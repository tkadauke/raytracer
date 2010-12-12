#include "OrthographicCamera.h"
#include "Buffer.h"
#include "Raytracer.h"
#include "Ray.h"
#include "ViewPlane.h"

void OrthographicCamera::render(Raytracer* raytracer, Buffer& buffer) {
  Matrix4d m = matrix();
  ViewPlane* plane = viewPlane();
  plane->setup(m, buffer.width(), buffer.height());

  Vector3d direction = Matrix3d(m) * Vector3d(0, 0, 1);

  for (ViewPlane::Iterator pixel = plane->begin(), end = plane->end(); pixel != end; ++pixel) {
    Ray ray(*pixel, direction);
    buffer[pixel.row()][pixel.column()] = raytracer->rayColor(ray);
    
    if (isCancelled())
      break;
  }

  uncancel();
}
