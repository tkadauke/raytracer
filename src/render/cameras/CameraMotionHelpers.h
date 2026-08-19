#pragma once

#include "render/animation/AnimationTrack.h"
#include "render/cameras/Camera.h"
#include "core/math/Matrix.h"
#include "core/math/Vector.h"

#include <algorithm>
#include <limits>
#include <optional>

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

  /// The animated position/target/direction endpoints at shutter open and
  /// close, shared by the pinhole and orthographic GPU motion-descriptor
  /// samplers.
  struct ShutterMotionEndpoints {
    Vector3d positionAtOpen;
    Vector3d positionAtClose;
    Vector3d targetAtOpen;
    Vector3d targetAtClose;
    Vector3d directionAtOpen;
    Vector3d directionAtClose;
  };

  /// Samples the "position"/"target" animation tracks at shutter open/close,
  /// returning `std::nullopt` if there is nothing to sample: no tracks, a
  /// non-linear track, a track with a keyframe inside the shutter interval,
  /// or a direction that becomes undefined (position == target) at either
  /// endpoint or anywhere on the linear segment between them.
  inline std::optional<ShutterMotionEndpoints>
  sampledLinearShutterMotionEndpoints(const Camera& camera) {
    const auto* positionTrack = camera.animationTrack("position");
    const auto* targetTrack = camera.animationTrack("target");
    if (!positionTrack && !targetTrack) {
      return std::nullopt;
    }
    if (!isLinearVectorTrack(positionTrack) || !isLinearVectorTrack(targetTrack)) {
      return std::nullopt;
    }

    const double shutterOpen = camera.animationTimeForSample(0.0);
    const double shutterClose = camera.animationTimeForSample(1.0);
    if ((positionTrack && crossesInteriorKey(*positionTrack, shutterOpen, shutterClose)) ||
        (targetTrack && crossesInteriorKey(*targetTrack, shutterOpen, shutterClose))) {
      return std::nullopt;
    }

    ShutterMotionEndpoints endpoints;
    endpoints.positionAtOpen = animatedVectorAt(camera, "position", camera.position(), shutterOpen);
    endpoints.positionAtClose =
      animatedVectorAt(camera, "position", camera.position(), shutterClose);
    endpoints.targetAtOpen = animatedVectorAt(camera, "target", camera.target(), shutterOpen);
    endpoints.targetAtClose = animatedVectorAt(camera, "target", camera.target(), shutterClose);
    endpoints.directionAtOpen = endpoints.targetAtOpen - endpoints.positionAtOpen;
    endpoints.directionAtClose = endpoints.targetAtClose - endpoints.positionAtClose;
    if (!hasDefinedDirection(endpoints.positionAtOpen, endpoints.targetAtOpen) ||
        !hasDefinedDirection(endpoints.positionAtClose, endpoints.targetAtClose) ||
        !linearDirectionSegmentStaysDefined(endpoints.directionAtOpen, endpoints.directionAtClose)) {
      return std::nullopt;
    }

    return endpoints;
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
