#include "raytracer/cameras/CameraFactory.h"
#include "raytracer/cameras/PinholeCamera.h"
#include "core/math/Ray.h"
#include "raytracer/viewplanes/ViewPlane.h"

using namespace raytracer;

Rayd PinholeCamera::rayForPixel(double x, double y, SampleStream&) const {
  Vector3d position = matrix() * Vector4d(0, 0, -m_distance);
  Vector3d pixel = viewPlane()->pixelAt(x, y);
  return Rayd(position, (pixel - position).normalized());
}

Vector2d PinholeCamera::projectPoint(const Vector3d& worldPoint) const {
  // Inverse of rayForPixel: transform the world point into camera
  // space, project through the pinhole at (0, 0, -distance), invert
  // the view-plane basis to recover pixel coordinates.
  Matrix4d worldToCamera = matrix().inverted();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  // Eye is at (0, 0, -distance) in camera space; the view plane is
  // the z=0 plane. A point at z_cam ≤ -distance is at or behind the
  // eye, so perspective projection is undefined.
  double denominator = pCam.z() + m_distance;
  if (denominator <= 0.0) {
    return Vector2d::undefined();
  }

  // Similar-triangles projection onto z=0:
  //   t = distance / (z_cam + distance)
  //   Q.x_cam = pCam.x * t,  Q.y_cam = pCam.y * t
  double t = m_distance / denominator;
  double qxCam = pCam.x() * t;
  double qyCam = pCam.y() * t;

  // The view plane setup (see ViewPlane::setupVectors) is:
  //   pixelAt(x, y)_cam = pixelSize * (-4 + 8x/width, -3 + 6y/height, 0)
  // Inverting:
  //   x = (qxCam / pixelSize + 4) * width / 8
  //   y = (qyCam / pixelSize + 3) * height / 6
  auto plane = viewPlane();
  double pxSize = plane->pixelSize();
  double pxCanon = qxCam / pxSize;
  double pyCanon = qyCam / pxSize;
  double x = (pxCanon + 4.0) * plane->width()  / 8.0;
  double y = (pyCanon + 3.0) * plane->height() / 6.0;
  return Vector2d(x, y);
}

void PinholeCamera::setViewPlane(std::shared_ptr<ViewPlane> plane) {
  Camera::setViewPlane(plane);
  viewPlane()->setPixelSize(1.0 / m_zoom);
}

static bool dummy = CameraFactory::self().registerClass<PinholeCamera>("PinholeCamera");
