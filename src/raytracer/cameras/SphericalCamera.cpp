#include "raytracer/cameras/CameraFactory.h"
#include "raytracer/cameras/SphericalCamera.h"
#include "core/math/Ray.h"
#include "core/math/Constants.h"
#include "raytracer/viewplanes/ViewPlane.h"

#include <cmath>

using namespace std;
using namespace raytracer;

Vector3d SphericalCamera::direction(double x, double y) const {
  Vector2d point(2.0 / viewPlane()->width() * x + 1.0, 2.0 / viewPlane()->height() * y - 1.0);

  double lambda = point.x() * 0.5 * m_horizontalFieldOfView.radians();
  double psi = point.y() * 0.5 * m_verticalFieldOfView.radians();
  double phi = PI - lambda;
  double theta = 0.5 * PI - psi;
  double sinPhi = sin(phi);
  double cosPhi = cos(phi);
  double sinTheta = sin(theta);
  double cosTheta = cos(theta);
  return Matrix3d(matrix()) * Vector3d(sinTheta * sinPhi, cosTheta, sinTheta * cosPhi);
}

Rayd SphericalCamera::rayForPixel(double x, double y) const {
  Vector3d position = matrix() * Vector4d(0, 0, -5);
  return Rayd(position, direction(x, y));
}

static bool dummy = CameraFactory::self().registerClass<SphericalCamera>("SphericalCamera");
