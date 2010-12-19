#include "cameras/SphericalCamera.h"
#include "Buffer.h"
#include "Raytracer.h"
#include "math/Ray.h"
#include "viewplanes/ViewPlane.h"

#include <cmath>

using namespace std;

void SphericalCamera::render(Raytracer* raytracer, Buffer& buffer, const Rect& rect) {
  Matrix4d m = matrix();
  ViewPlane* plane = viewPlane();

  Vector3d position = m * Vector3d(0, 0, -5);

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
