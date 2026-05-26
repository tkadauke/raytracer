#include "world/objects/ElementFactory.h"
#include "world/objects/PinholeCamera.h"

#include "render/cameras/PinholeCamera.h"

#include <algorithm>
#include <cmath>

namespace {
  constexpr double kNominalHalfWidth = 4.0;
  constexpr double kNominalHalfHeight = 3.0;
  constexpr double kFrameMargin = 1.15;

  Vector3d normalizedOrDefault(const Vector3d& direction) {
    if (direction.isUndefined() || direction.length() <= 1e-9) {
      return Vector3d(0.75, 0.45, -1.0).normalized();
    }
    return direction.normalized();
  }

  Vector3d cameraRightFor(const Vector3d& forward) {
    Vector3d right = Vector3d::up() ^ forward;
    if (right.length() <= 1e-9) {
      right = Vector3d(1.0, 0.0, 0.0) ^ forward;
    }
    return right.normalized();
  }
}

PinholeCamera::PinholeCamera(Element* parent)
    : Camera(parent),
      m_distance(5),
      m_zoom(1) {
}

bool PinholeCamera::frame(const BoundingBoxd& bounds) {
  return frameFrom(bounds, position() - target());
}

bool PinholeCamera::frameFrom(const BoundingBoxd& bounds, const Vector3d& targetToEyeDirection) {
  if (!bounds.isValid() || bounds.isInfinite())
    return false;

  const double lensDistance = distance() > 0.0 && std::isfinite(distance()) ? distance() : 5.0;
  const double zoomFactor = zoom() > 0.0 && std::isfinite(zoom()) ? zoom() : 1.0;
  const double halfWidth = kNominalHalfWidth / zoomFactor / kFrameMargin;
  const double halfHeight = kNominalHalfHeight / zoomFactor / kFrameMargin;
  if (halfWidth <= 0.0 || halfHeight <= 0.0)
    return false;

  const Vector3d target = bounds.center();
  const Vector3d eyeDirection = normalizedOrDefault(targetToEyeDirection);
  const Vector3d forward = -eyeDirection;
  const Vector3d right = cameraRightFor(forward);
  const Vector3d up = right ^ -forward;

  double eyeToTarget = lensDistance + 1.0;
  for (const Vector3d& corner : bounds.vertices()) {
    const Vector3d relative = corner - target;
    const double x = std::abs(relative * right);
    const double y = std::abs(relative * up);
    const double z = relative * forward;
    eyeToTarget = std::max(eyeToTarget, lensDistance * x / halfWidth - z);
    eyeToTarget = std::max(eyeToTarget, lensDistance * y / halfHeight - z);
    eyeToTarget = std::max(eyeToTarget, -z + lensDistance + 1.0);
  }

  setDistance(lensDistance);
  setTarget(target);
  setPosition(target + eyeDirection * std::max(eyeToTarget - lensDistance, 1.0));
  setZoom(zoomFactor);
  return true;
}

std::shared_ptr<render::Camera> PinholeCamera::toRaytracer() const {
  auto camera = make_named<render::PinholeCamera>(position(), target());
  camera->setDistance(distance());
  camera->setZoom(zoom());
  return camera;
}

static bool dummy = ElementFactory::self().registerClass<PinholeCamera>("PinholeCamera");
