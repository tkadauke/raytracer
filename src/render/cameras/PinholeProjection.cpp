#include "PinholeProjection.h"

#include "render/cameras/Camera.h"
#include "render/viewplanes/ViewPlane.h"

using namespace render;
using namespace render::detail;

PinholeProjection::PinholeProjection(const Camera& camera, double distance)
    : m_camera(camera),
      m_distance(distance) {
}

Vector2d PinholeProjection::projectPoint(const Vector3d& worldPoint) const {
  // Inverse of PinholeCamera::rayForPixel. With ViewPlane::pixelAt's
  // standard convention, the view plane in camera space is at z=0
  // regardless of pixelSize.
  const Matrix4d& worldToCamera = m_camera.inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  // Eye is at (0, 0, -distance) in camera space; perspective
  // projection diverges at or behind the eye.
  double denominator = pCam.z() + m_distance;
  if (denominator <= 0.0) {
    return Vector2d::undefined;
  }

  double t = m_distance / denominator;
  double qxCam = pCam.x() * t;
  double qyCam = pCam.y() * t;

  auto plane = m_camera.viewPlane();
  double pxSize = plane->pixelSize();
  double halfH = plane->hSpan() / 2.0;
  double halfV = plane->vSpan() / 2.0;
  double lx = qxCam / pxSize;
  double ly = qyCam / pxSize;

  const Recti& inner = plane->innerRect();
  double x = (lx + halfH) * inner.width() / plane->hSpan() + inner.left();
  double y = (ly + halfV) * inner.height() / plane->vSpan() + inner.top();
  return Vector2d(x, y);
}

Vector3d PinholeProjection::projectPointWithDepth(const Vector3d& worldPoint) const {
  const Matrix4d& worldToCamera = m_camera.inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  double depth = pCam.z() + m_distance;
  if (depth <= 0.0) {
    return Vector3d::undefined;
  }

  double t = m_distance / depth;
  double qxCam = pCam.x() * t;
  double qyCam = pCam.y() * t;

  auto plane = m_camera.viewPlane();
  double pxSize = plane->pixelSize();
  double halfH = plane->hSpan() / 2.0;
  double halfV = plane->vSpan() / 2.0;
  double lx = qxCam / pxSize;
  double ly = qyCam / pxSize;

  const Recti& inner = plane->innerRect();
  double x = (lx + halfH) * inner.width() / plane->hSpan() + inner.left();
  double y = (ly + halfV) * inner.height() / plane->vSpan() + inner.top();
  return Vector3d(x, y, depth);
}

Matrix4d PinholeProjection::projectionMatrix() const {
  auto plane = m_camera.viewPlane();
  const double halfW = plane->hSpan() * plane->pixelSize() / 2.0;
  const double halfH = plane->vSpan() * plane->pixelSize() / 2.0;
  return Matrix4d::frustum(-halfW, halfW, -halfH, halfH, m_distance, 1e6);
}

Vector4d PinholeProjection::projectPointToClipSpace(const Vector3d& worldPoint) const {
  const Matrix4d& worldToCamera = m_camera.inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  // Shift z so the eye is at the origin before applying the frustum matrix.
  const double depth = pCam.z() + m_distance;
  const Vector4d v = projectionMatrix() * Vector4d(pCam.x(), pCam.y(), depth, 1.0);
  // The rasterizer's homogeneous clipper and depth buffer operate in
  // eye-space units, so z and w intentionally carry raw eye depth.
  return Vector4d(v.x(), v.y(), depth, depth);
}

double PinholeProjection::eyeRelativeDepth(const Vector3d& worldPoint) const {
  const Matrix4d& worldToCamera = m_camera.inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);
  return pCam.z() + m_distance;
}
