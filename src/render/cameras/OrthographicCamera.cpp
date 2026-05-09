#include "render/cameras/CameraFactory.h"
#include "render/cameras/OrthographicCamera.h"
#include "core/math/Ray.h"
#include "render/viewplanes/ViewPlane.h"

using namespace render;

Rayd OrthographicCamera::rayForPixel(double x, double y, render::SampleStream&) const {
  Vector3d direction = Matrix3d(matrix()) * Vector3d::forward();
  Vector3d pixel = viewPlane()->pixelAt(x, y);
  return Rayd(pixel, direction);
}

Vector2d OrthographicCamera::projectPoint(const Vector3d& worldPoint) const {
  // Orthographic projection: drop the camera-forward axis component
  // and convert the remaining camera-space (x, y) directly into
  // pixel coordinates. No perspective divide — every world point
  // projects via parallel rays.
  const Matrix4d& worldToCamera = inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  if (pCam.z() < 0.0) {
    return Vector2d::undefined();
  }

  // Same view-plane mapping as PinholeCamera: the camera-space plane
  // spans (-4, -3, 0)..(+4, +3, 0) before pixelSize scaling.
  auto plane = viewPlane();
  double pxSize = plane->pixelSize();
  double lx = pCam.x() / pxSize;
  double ly = pCam.y() / pxSize;
  double x = (lx + 4.0) * plane->width()  / 8.0;
  double y = (ly + 3.0) * plane->height() / 6.0;
  return Vector2d(x, y);
}

Vector3d OrthographicCamera::projectPointWithDepth(const Vector3d& worldPoint) const {
  const Matrix4d& worldToCamera = inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  if (pCam.z() < 0.0) {
    return Vector3d::undefined();
  }

  auto plane = viewPlane();
  double pxSize = plane->pixelSize();
  double lx = pCam.x() / pxSize;
  double ly = pCam.y() / pxSize;
  double x = (lx + 4.0) * plane->width()  / 8.0;
  double y = (ly + 3.0) * plane->height() / 6.0;
  // For orthographic projection, depth IS the camera-space z; no
  // m_distance offset since there's no perspective eye point. The
  // rasterizer uses clip.w for projective interpolation, and our
  // clip.w is always 1, so depth and attributes interpolate linearly.
  return Vector3d(x, y, pCam.z());
}

Vector4d OrthographicCamera::projectPointToClipSpace(const Vector3d& worldPoint) const {
  const Matrix4d& worldToCamera = inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  const double pxSize = viewPlane()->pixelSize();
  return Vector4d(
    pCam.x() / (pxSize * 4.0),
    pCam.y() / (pxSize * 3.0),
    pCam.z(),
    1.0
  );
}

double OrthographicCamera::eyeRelativeDepth(const Vector3d& worldPoint) const {
  // Orthographic projection has no perspective eye point; depth is
  // just the camera-space `z` coordinate. Positive in front of the
  // viewplane, negative behind.
  const Matrix4d& worldToCamera = inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);
  return pCam.z();
}

void OrthographicCamera::setViewPlane(std::shared_ptr<render::ViewPlane> plane) {
  Camera::setViewPlane(plane);
  viewPlane()->setPixelSize(1.0 / m_zoom);
}

static bool dummy = CameraFactory::self().registerClass<OrthographicCamera>("OrthographicCamera");
