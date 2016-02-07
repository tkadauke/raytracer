#include "raytracer/cameras/CameraFactory.h"
#include "raytracer/cameras/SphericalCamera.h"
#include "core/Buffer.h"
#include "raytracer/Raytracer.h"
#include "core/math/Ray.h"
#include "raytracer/viewplanes/ViewPlane.h"

#include <cmath>

using namespace std;
using namespace raytracer;

void SphericalCamera::render(Raytracer* raytracer, Buffer<unsigned int>& buffer, const Rect& rect) {
  auto plane = viewPlane();

  Vector3d position = matrix() * Vector4d(0, 0, -5);

  for (ViewPlane::Iterator pixel = plane->begin(rect), end = plane->end(rect); pixel != end; ++pixel) {
    Ray ray(position, direction(*plane, pixel.column(), pixel.row()));
    plot(buffer, rect, pixel, raytracer->rayColor(ray));
    
    if (isCancelled())
      break;
  }
}

Vector3d SphericalCamera::direction(const ViewPlane& plane, int x, int y) {
  Vector2d point(2.0 / plane.width() * x + 1.0, 2.0 / plane.height() * y - 1.0);

  double lambda = point.x() * 0.5 * m_horizontalFieldOfView * (3.14159265 / 180.0);
  double psi = point.y() * 0.5 * m_verticalFieldOfView * (3.14159265 / 180.0);
  double phi = 3.14159265 - lambda;
  double theta = 0.5 * 3.14159265 - psi;
  double sinPhi = sin(phi);
  double cosPhi = cos(phi);
  double sinTheta = sin(theta);
  double cosTheta = cos(theta);
  return Matrix3d(matrix()) * Vector3d(sinTheta * sinPhi, cosTheta, sinTheta * cosPhi);
}

Ray SphericalCamera::rayForPixel(int x, int y) {
  Vector3d position = matrix() * Vector4d(0, 0, -5);
  return Ray(position, direction(*viewPlane(), x, y));
}

static bool dummy = CameraFactory::self().registerClass<SphericalCamera>("SphericalCamera");
