#include "world/objects/ElementFactory.h"
#include "world/objects/PinholeCamera.h"

#include "render/cameras/PinholeCamera.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
  constexpr double kNominalHalfWidth = 4.0;
  constexpr double kNominalHalfHeight = 3.0;
  constexpr double kFrameMargin = 1.15;
  constexpr double kMinimumViewPlaneTargetGap = 1.0;

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

  double minimumEyeToTargetDistance(const BoundingBoxd& bounds, const Vector3d& target,
                                    const Vector3d& forward, double lensDistance) {
    double result = lensDistance + kMinimumViewPlaneTargetGap;
    for (const Vector3d& corner : bounds.vertices()) {
      const double z = (corner - target) * forward;
      result = std::max(result, -z + lensDistance + kMinimumViewPlaneTargetGap);
    }
    return result;
  }

  double zoomThatFitsAtDistance(const BoundingBoxd& bounds, const Vector3d& target,
                                const Vector3d& forward, const Vector3d& right, const Vector3d& up,
                                double lensDistance, double eyeToTarget) {
    double result = std::numeric_limits<double>::infinity();
    for (const Vector3d& corner : bounds.vertices()) {
      const Vector3d relative = corner - target;
      const double availableDepth = eyeToTarget + relative * forward;
      if (availableDepth <= 0.0)
        continue;

      const double x = std::abs(relative * right);
      if (x > 1e-9) {
        result =
          std::min(result, availableDepth * kNominalHalfWidth / (lensDistance * x * kFrameMargin));
      }

      const double y = std::abs(relative * up);
      if (y > 1e-9) {
        result =
          std::min(result, availableDepth * kNominalHalfHeight / (lensDistance * y * kFrameMargin));
      }
    }
    return std::isfinite(result) && result > 0.0 ? result : 1.0;
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

  const Vector3d target = bounds.center();
  const Vector3d eyeDirection = normalizedOrDefault(targetToEyeDirection);
  const Vector3d forward = -eyeDirection;
  const Vector3d right = cameraRightFor(forward);
  const Vector3d up = right ^ -forward;
  const double minimumEyeToTarget =
    minimumEyeToTargetDistance(bounds, target, forward, lensDistance);
  const double fittedZoom =
    std::max(zoomFactor, zoomThatFitsAtDistance(bounds, target, forward, right, up, lensDistance,
                                                minimumEyeToTarget));
  const double halfWidth = kNominalHalfWidth / fittedZoom / kFrameMargin;
  const double halfHeight = kNominalHalfHeight / fittedZoom / kFrameMargin;
  if (halfWidth <= 0.0 || halfHeight <= 0.0)
    return false;

  double eyeToTarget = minimumEyeToTarget;
  for (const Vector3d& corner : bounds.vertices()) {
    const Vector3d relative = corner - target;
    const double x = std::abs(relative * right);
    const double y = std::abs(relative * up);
    const double z = relative * forward;
    eyeToTarget = std::max(eyeToTarget, lensDistance * x / halfWidth - z);
    eyeToTarget = std::max(eyeToTarget, lensDistance * y / halfHeight - z);
    eyeToTarget = std::max(eyeToTarget, -z + lensDistance + kMinimumViewPlaneTargetGap);
  }

  setDistance(lensDistance);
  setTarget(target);
  setPosition(target + eyeDirection * std::max(eyeToTarget - lensDistance, 1.0));
  setZoom(fittedZoom);
  return true;
}

std::shared_ptr<render::Camera> PinholeCamera::toRaytracer() const {
  auto camera = make_named<render::PinholeCamera>(position(), target());
  camera->setDistance(distance());
  camera->setZoom(zoom());
  applyCameraProperties(camera);
  return camera;
}

static bool dummy = ElementFactory::self().registerClass<PinholeCamera>("PinholeCamera");
