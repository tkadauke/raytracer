#include "raytracer/cameras/CameraFactory.h"
#include "raytracer/cameras/FishEyeCamera.h"
#include "core/math/Ray.h"
#include "raytracer/viewplanes/ViewPlane.h"

#include <cmath>

using namespace std;
using namespace raytracer;

Vector3d FishEyeCamera::direction(double x, double y) const {
  Vector2d point(2.0 / viewPlane()->width() * x - 1.0, 2.0 / viewPlane()->height() * y - 1.0);
  double r2 = point * point;
  if (r2 <= 1.0) {
    double r = sqrt(r2);
    double psi = r * m_fieldOfView.radians() / 2;
    double sinPsi = sin(psi);
    double cosPsi = cos(psi);
    double sinAlpha = point.y() / r;
    double cosAlpha = point.x() / r;
    return Matrix3d(matrix()) * Vector3d(sinPsi * cosAlpha, sinPsi * sinAlpha, cosPsi);
  } else
    return Vector3d::undefined();
}

Rayd FishEyeCamera::rayForPixel(double x, double y) const {
  Vector3d position = matrix().translationVector();
  return Rayd(position, direction(x, y));
}

static bool dummy = CameraFactory::self().registerClass<FishEyeCamera>("FishEyeCamera");
