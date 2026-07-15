#pragma once

#include "render/animation/AnimationTrack.h"
#include "render/cameras/Camera.h"
#include "core/math/Matrix.h"
#include "core/math/Vector.h"

#include <algorithm>
#include <limits>

namespace render::detail {
  inline bool isLinearVectorTrack(const render::animation::AnimationTrack* track) {
    return !track ||
           (track->interpolationMode() == core::math::interpolation::InterpolationMode::Linear &&
            track->valueType() == render::animation::AnimationValue::Type::Vector3);
  }

  inline bool crossesInteriorKey(const render::animation::AnimationTrack& track, double from,
                                  double to) {
    const double start = std::min(from, to);
    const double end = std::max(from, to);
    return std::any_of(track.keyframes().begin(), track.keyframes().end(),
                       [start, end](const auto& keyframe) {
                         return keyframe.time > start && keyframe.time < end;
                       });
  }

  inline Vector3d animatedVectorAt(const Camera& camera, const char* property,
                                    const Vector3d& fallback, double time) {
    const auto* track = camera.animationTrack(property);
    if (!track) {
      return fallback;
    }
    return fallback + track->sample(time).get<Vector3d>() -
           track->sample(camera.animationFrame()).get<Vector3d>();
  }

  inline bool nearlyEqual(const Vector3d& left, const Vector3d& right) {
    return (left - right).length() <= 1e-9;
  }

  inline bool hasDefinedDirection(const Vector3d& position, const Vector3d& target) {
    return (target - position).length() > std::numeric_limits<double>::epsilon();
  }

  inline bool linearDirectionSegmentStaysDefined(const Vector3d& directionAtOpen,
                                                  const Vector3d& directionAtClose) {
    const Vector3d directionDelta = directionAtClose - directionAtOpen;
    const double deltaLengthSquared = directionDelta * directionDelta;
    double closestT = 0.0;
    if (deltaLengthSquared > std::numeric_limits<double>::epsilon()) {
      closestT = std::clamp(-(directionAtOpen * directionDelta) / deltaLengthSquared, 0.0, 1.0);
    }
    return (directionAtOpen + directionDelta * closestT).length() >
           std::numeric_limits<double>::epsilon();
  }

  inline bool linearDirectionSegmentStaysOffUpAxis(const Vector3d& directionAtOpen,
                                                    const Vector3d& directionAtClose) {
    const Vector3d horizontalAtOpen(directionAtOpen.x(), 0.0, directionAtOpen.z());
    const Vector3d horizontalAtClose(directionAtClose.x(), 0.0, directionAtClose.z());
    const Vector3d horizontalDelta = horizontalAtClose - horizontalAtOpen;
    const double deltaLengthSquared = horizontalDelta * horizontalDelta;
    double closestT = 0.0;
    if (deltaLengthSquared > std::numeric_limits<double>::epsilon()) {
      closestT =
        std::clamp(-(horizontalAtOpen * horizontalDelta) / deltaLengthSquared, 0.0, 1.0);
    }
    return (horizontalAtOpen + horizontalDelta * closestT).length() >
           std::numeric_limits<double>::epsilon();
  }

  inline bool hasStableBasis(const Matrix4d& openMatrix, const Matrix4d& closeMatrix) {
    const Vector3d openRight =
      openMatrix.transformDirection(Vector3d(1.0, 0.0, 0.0)).normalized();
    const Vector3d closeRight =
      closeMatrix.transformDirection(Vector3d(1.0, 0.0, 0.0)).normalized();
    const Vector3d openDown = openMatrix.transformDirection(Vector3d(0.0, 1.0, 0.0)).normalized();
    const Vector3d closeDown =
      closeMatrix.transformDirection(Vector3d(0.0, 1.0, 0.0)).normalized();
    const Vector3d openForward = openMatrix.transformDirection(Vector3d::forward()).normalized();
    const Vector3d closeForward = closeMatrix.transformDirection(Vector3d::forward()).normalized();
    return openRight.isDefined() && closeRight.isDefined() && openDown.isDefined() &&
           closeDown.isDefined() && openForward.isDefined() && closeForward.isDefined() &&
           nearlyEqual(openRight, closeRight) && nearlyEqual(openDown, closeDown) &&
           nearlyEqual(openForward, closeForward);
  }
}
