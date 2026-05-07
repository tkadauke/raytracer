#include "render/cameras/CameraFactory.h"
#include "render/cameras/PinholeCamera.h"
#include "core/math/Ray.h"
#include "render/viewplanes/ViewPlane.h"

using namespace render;

Rayd PinholeCamera::rayForPixel(double x, double y, render::SampleStream&) const {
  Vector3d position = matrix() * Vector4d(0, 0, -m_distance);
  Vector3d pixel = viewPlane()->pixelAt(x, y);
  return Rayd(position, (pixel - position).normalized());
}

Vector2d PinholeCamera::projectPoint(const Vector3d& worldPoint) const {
  // Inverse of rayForPixel. With `pixelAt`'s standard convention
  // (view plane scaled around the camera position), the view plane
  // in camera space is at z=0 regardless of pixelSize, so the
  // inversion is straightforward: project pCam through eye=(0,0,-d)
  // onto z=0, then convert camera-space plane coords to pixels.
  Matrix4d worldToCamera = matrix().inverted();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  // Eye is at (0, 0, -distance) in camera space; perspective
  // projection diverges at or behind the eye.
  double denominator = pCam.z() + m_distance;
  if (denominator <= 0.0) {
    return Vector2d::undefined();
  }

  // Similar-triangles projection onto the camera-space z=0 plane.
  double t = m_distance / denominator;
  double qxCam = pCam.x() * t;
  double qyCam = pCam.y() * t;

  // The view plane in camera space spans (-4, -3, 0) to (+4, +3, 0)
  // before pixelSize scaling; the pixelSize factor scales the
  // camera-space plane coords directly.
  auto plane = viewPlane();
  double pxSize = plane->pixelSize();
  double lx = qxCam / pxSize;
  double ly = qyCam / pxSize;
  double x = (lx + 4.0) * plane->width()  / 8.0;
  double y = (ly + 3.0) * plane->height() / 6.0;
  return Vector2d(x, y);
}

Vector3d PinholeCamera::projectPointWithDepth(const Vector3d& worldPoint) const {
  // Same projection math as projectPoint above, additionally
  // returning the eye-relative distance along the camera's forward
  // axis. Used by the software rasterizer's Z-buffer for depth
  // tests and by perspective-correct attribute interpolation.
  Matrix4d worldToCamera = matrix().inverted();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  // The eye sits at camera-space (0, 0, -distance); a world point at
  // camera-space depth `pCam.z()` is `pCam.z() + distance` units
  // from the eye along the forward axis. Behind the eye → undefined.
  double depth = pCam.z() + m_distance;
  if (depth <= 0.0) {
    return Vector3d::undefined();
  }

  double t = m_distance / depth;
  double qxCam = pCam.x() * t;
  double qyCam = pCam.y() * t;

  auto plane = viewPlane();
  double pxSize = plane->pixelSize();
  double lx = qxCam / pxSize;
  double ly = qyCam / pxSize;
  double x = (lx + 4.0) * plane->width()  / 8.0;
  double y = (ly + 3.0) * plane->height() / 6.0;
  return Vector3d(x, y, depth);
}

void PinholeCamera::setViewPlane(std::shared_ptr<render::ViewPlane> plane) {
  Camera::setViewPlane(plane);
  viewPlane()->setPixelSize(1.0 / m_zoom);
}

static bool dummy = CameraFactory::self().registerClass<render::PinholeCamera>("PinholeCamera");
