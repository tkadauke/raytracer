#include "raytracer/cameras/CameraFactory.h"
#include "raytracer/cameras/OrthographicCamera.h"
#include "core/Buffer.h"
#include "raytracer/Raytracer.h"
#include "core/math/Ray.h"
#include "raytracer/viewplanes/ViewPlane.h"

using namespace raytracer;

void OrthographicCamera::render(std::shared_ptr<Raytracer> raytracer, Buffer<unsigned int>& buffer, const Rect& rect) {
  auto plane = viewPlane();

  Vector3d direction = Matrix3d(matrix()) * Vector3d::forward();

  for (ViewPlane::Iterator pixel = plane->begin(rect), end = plane->end(rect); pixel != end; ++pixel) {
    Ray ray(*pixel, direction);
    plot(buffer, rect, pixel, raytracer->rayColor(ray));
    
    if (isCancelled())
      break;
  }
}

Ray OrthographicCamera::rayForPixel(int x, int y) {
  Vector3d direction = Matrix3d(matrix()) * Vector3d::forward();
  Vector3d pixel = viewPlane()->pixelAt(x, y);
  return Ray(pixel, direction);
}

static bool dummy = CameraFactory::self().registerClass<OrthographicCamera>("OrthographicCamera");
