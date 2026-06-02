#include "render/cameras/CameraFactory.h"
#include "render/cameras/OrthographicCamera.h"
#include "core/math/Ray.h"
#include "render/viewplanes/ViewPlane.h"

using namespace render;

std::shared_ptr<Camera> OrthographicCamera::clone() const {
  auto result = std::make_shared<OrthographicCamera>();
  copyBaseStateTo(*result);
  result->m_zoom = m_zoom;
  return result;
}

const char* OrthographicCamera::fingerprintType() const {
  return "OrthographicCamera";
}

Rayd OrthographicCamera::rayForPixel(double x, double y, render::SampleStream&) const {
  Vector3d direction = matrix().transformDirection(Vector3d::forward());
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
    return Vector2d::undefined;
  }

  auto plane = viewPlane();
  double pxSize = plane->pixelSize();
  double halfH = plane->hSpan() / 2.0;
  double halfV = plane->vSpan() / 2.0;
  double lx = pCam.x() / pxSize;
  double ly = pCam.y() / pxSize;
  const Recti& inner = plane->innerRect();
  double x = (lx + halfH) * inner.width() / plane->hSpan() + inner.left();
  double y = (ly + halfV) * inner.height() / plane->vSpan() + inner.top();
  return Vector2d(x, y);
}

Vector3d OrthographicCamera::projectPointWithDepth(const Vector3d& worldPoint) const {
  const Matrix4d& worldToCamera = inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  if (pCam.z() < 0.0) {
    return Vector3d::undefined;
  }

  auto plane = viewPlane();
  double pxSize = plane->pixelSize();
  double halfH = plane->hSpan() / 2.0;
  double halfV = plane->vSpan() / 2.0;
  double lx = pCam.x() / pxSize;
  double ly = pCam.y() / pxSize;
  const Recti& inner = plane->innerRect();
  double x = (lx + halfH) * inner.width() / plane->hSpan() + inner.left();
  double y = (ly + halfV) * inner.height() / plane->vSpan() + inner.top();
  // For orthographic projection, depth IS the camera-space z; no
  // m_distance offset since there's no perspective eye point. The
  // rasterizer uses clip.w for projective interpolation, and our
  // clip.w is always 1, so depth and attributes interpolate linearly.
  return Vector3d(x, y, pCam.z());
}

Matrix4d OrthographicCamera::projectionMatrix() const {
  auto plane = viewPlane();
  const double halfW = plane->hSpan() * plane->pixelSize() / 2.0;
  const double halfH = plane->vSpan() * plane->pixelSize() / 2.0;
  return Matrix4d::orthographic(-halfW, halfW, -halfH, halfH, 0.0, 1e6);
}

Vector4d OrthographicCamera::projectPointToClipSpace(const Vector3d& worldPoint) const {
  const Matrix4d& worldToCamera = inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  const Vector4d v = projectionMatrix() * Vector4d(pCam.x(), pCam.y(), pCam.z(), 1.0);
  // Use camera-space z for the depth component: the rasterizer's
  // HomogeneousClipVolume depth planes and depth buffer use it directly in
  // eye-space units rather than the NDC z the orthographic factory produces.
  return Vector4d(v.x(), v.y(), pCam.z(), 1.0);
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
