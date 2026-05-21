#include "render/cameras/CameraFactory.h"
#include "render/cameras/PinholeCamera.h"
#include "core/math/Ray.h"
#include "render/viewplanes/ViewPlane.h"

using namespace render;

std::shared_ptr<Camera> PinholeCamera::clone() const {
  auto result = std::make_shared<PinholeCamera>();
  copyBaseStateTo(*result);
  result->m_distance = m_distance;
  result->m_zoom = m_zoom;
  return result;
}

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
  const Matrix4d& worldToCamera = inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  // Eye is at (0, 0, -distance) in camera space; perspective
  // projection diverges at or behind the eye.
  double denominator = pCam.z() + m_distance;
  if (denominator <= 0.0) {
    return Vector2d::undefined;
  }

  // Similar-triangles projection onto the camera-space z=0 plane.
  double t = m_distance / denominator;
  double qxCam = pCam.x() * t;
  double qyCam = pCam.y() * t;

  auto plane = viewPlane();
  double pxSize = plane->pixelSize();
  double halfH = plane->hSpan() / 2.0;
  double halfV = plane->vSpan() / 2.0;
  double lx = qxCam / pxSize;
  double ly = qyCam / pxSize;

  // For FitExact the renderable area is the inner rect; map to that
  // sub-rectangle and then offset into the full buffer.
  const Recti& inner = plane->innerRect();
  double x = (lx + halfH) * inner.width()  / plane->hSpan() + inner.left();
  double y = (ly + halfV) * inner.height() / plane->vSpan() + inner.top();
  return Vector2d(x, y);
}

Vector3d PinholeCamera::projectPointWithDepth(const Vector3d& worldPoint) const {
  // Same projection math as projectPoint above, additionally
  // returning the eye-relative distance along the camera's forward
  // axis. Used by the software rasterizer's Z-buffer for depth
  // tests and by perspective-correct attribute interpolation.
  const Matrix4d& worldToCamera = inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  // The eye sits at camera-space (0, 0, -distance); a world point at
  // camera-space depth `pCam.z()` is `pCam.z() + distance` units
  // from the eye along the forward axis. Behind the eye → undefined.
  double depth = pCam.z() + m_distance;
  if (depth <= 0.0) {
    return Vector3d::undefined;
  }

  double t = m_distance / depth;
  double qxCam = pCam.x() * t;
  double qyCam = pCam.y() * t;

  auto plane = viewPlane();
  double pxSize = plane->pixelSize();
  double halfH = plane->hSpan() / 2.0;
  double halfV = plane->vSpan() / 2.0;
  double lx = qxCam / pxSize;
  double ly = qyCam / pxSize;

  const Recti& inner = plane->innerRect();
  double x = (lx + halfH) * inner.width()  / plane->hSpan() + inner.left();
  double y = (ly + halfV) * inner.height() / plane->vSpan() + inner.top();
  return Vector3d(x, y, depth);
}

Matrix4d PinholeCamera::projectionMatrix() const {
  auto plane = viewPlane();
  const double halfW = plane->hSpan() * plane->pixelSize() / 2.0;
  const double halfH = plane->vSpan() * plane->pixelSize() / 2.0;
  return Matrix4d::frustum(-halfW, halfW, -halfH, halfH, m_distance, 1e6);
}

Vector4d PinholeCamera::projectPointToClipSpace(const Vector3d& worldPoint) const {
  const Matrix4d& worldToCamera = inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  // Eye sits at camera-space (0,0,-distance); shift z so the eye is at the
  // origin before applying the frustum projection matrix.
  const double depth = pCam.z() + m_distance;
  const Vector4d v = projectionMatrix() * Vector4d(pCam.x(), pCam.y(), depth, 1.0);
  // Use raw eye depth for both z and w: the rasterizer's HomogeneousClipVolume
  // near test and depth buffer operate in eye-space units, not NDC z.
  return Vector4d(v.x(), v.y(), depth, depth);
}

double PinholeCamera::eyeRelativeDepth(const Vector3d& worldPoint) const {
  // The eye sits at camera-space (0, 0, -distance); a world point at
  // camera-space depth `pCam.z()` is `pCam.z() + distance` units
  // from the eye along the forward axis. Negative values indicate
  // points behind the eye — which the rasterizer's clipper trims to
  // the near plane rather than dropping the whole containing
  // triangle.
  const Matrix4d& worldToCamera = inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);
  return pCam.z() + m_distance;
}

void PinholeCamera::setViewPlane(std::shared_ptr<render::ViewPlane> plane) {
  Camera::setViewPlane(plane);
  viewPlane()->setPixelSize(1.0 / m_zoom);
}

static bool dummy = CameraFactory::self().registerClass<render::PinholeCamera>("PinholeCamera");
