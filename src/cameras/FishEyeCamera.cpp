#include "cameras/CameraFactory.h"
#include "cameras/FishEyeCamera.h"
#include "Buffer.h"
#include "Raytracer.h"
#include "math/Ray.h"
#include "viewplanes/ViewPlane.h"

#include <cmath>

using namespace std;

void FishEyeCamera::render(Raytracer* raytracer, Buffer<unsigned int>& buffer, const Rect& rect) {
  Matrix4d m = matrix();
  ViewPlane* plane = viewPlane();

  Vector3d position = m * Vector4d(0, 0, 0);

  for (ViewPlane::Iterator pixel = plane->begin(rect), end = plane->end(rect); pixel != end; ++pixel) {
    Ray ray(position, direction(*plane, pixel.column(), pixel.row()));
    if (ray.direction().isDefined())
      plot(buffer, rect, pixel, raytracer->rayColor(ray));
    else
      plot(buffer, rect, pixel, Colord::black());
    
    if (isCancelled())
      break;
  }
}

Vector3d FishEyeCamera::direction(const ViewPlane& plane, int x, int y) {
  Vector2d point(2.0 / plane.width() * x - 1.0, 2.0 / plane.height() * y - 1.0);
  double r2 = point * point;
  if (r2 <= 1.0) {
    double r = sqrt(r2);
    double psi = r / 2.0 * m_fieldOfView * (3.14159265 / 180.0);
    double sinPsi = sin(psi);
    double cosPsi = cos(psi);
    double sinAlpha = point.y() / r;
    double cosAlpha = point.x() / r;
    return Matrix3d(matrix()) * Vector3d(sinPsi * cosAlpha, sinPsi * sinAlpha, cosPsi);
  } else
    return Vector3d::undefined();
}

Ray FishEyeCamera::rayForPixel(int x, int y) {
  Vector3d position = matrix() * Vector4d(0, 0, 0);
  return Ray(position, direction(*viewPlane(), x, y));
}

static bool dummy = CameraFactory::self().registerClass<FishEyeCamera>("FishEyeCamera");
