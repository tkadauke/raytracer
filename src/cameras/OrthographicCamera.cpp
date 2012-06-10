#include "cameras/CameraFactory.h"
#include "cameras/OrthographicCamera.h"
#include "Buffer.h"
#include "Raytracer.h"
#include "math/Ray.h"
#include "viewplanes/ViewPlane.h"

void OrthographicCamera::render(Raytracer* raytracer, Buffer<unsigned int>& buffer, const Rect& rect) {
  Matrix4d m = matrix();
  ViewPlane* plane = viewPlane();

  Vector3d direction = Matrix3d(m) * Vector3d(0, 0, 1);

  for (ViewPlane::Iterator pixel = plane->begin(rect), end = plane->end(rect); pixel != end; ++pixel) {
    Ray ray(*pixel, direction);
    plot(buffer, rect, pixel, raytracer->rayColor(ray));
    
    if (isCancelled())
      break;
  }
}

static bool dummy = CameraFactory::self().registerClass<OrthographicCamera>("OrthographicCamera");
