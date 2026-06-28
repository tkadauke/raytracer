#include "render/cameras/SampledShutterDescriptorMotion.h"

#include "render/animation/AnimationTrack.h"
#include "render/cameras/Camera.h"

#include <algorithm>
#include <limits>

namespace render::detail {
  namespace {
    bool isLinearVectorTrack(const render::animation::AnimationTrack* track) {
      return !track ||
             (track->interpolationMode() == core::math::interpolation::InterpolationMode::Linear &&
              track->valueType() == render::animation::AnimationValue::Type::Vector3);
    }

    bool crossesInteriorKey(const render::animation::AnimationTrack& track, double from,
                            double to) {
      const double start = std::min(from, to);
      const double end = std::max(from, to);
      return std::any_of(track.keyframes().begin(), track.keyframes().end(),
                         [start, end](const auto& keyframe) {
                           return keyframe.time > start && keyframe.time < end;
                         });
    }

    Vector3d animatedVectorAt(const Camera& camera, const char* property, const Vector3d& fallback,
                              double time) {
      const auto* track = camera.animationTrack(property);
      if (!track) {
        return fallback;
      }
      return fallback + track->sample(time).get<Vector3d>() -
             track->sample(camera.animationFrame()).get<Vector3d>();
    }

    bool hasDefinedDirection(const Vector3d& position, const Vector3d& target) {
      return (target - position).length() > std::numeric_limits<double>::epsilon();
    }

    bool nearlyEqual(const Vector3d& left, const Vector3d& right) {
      return (left - right).length() <= 1e-9;
    }

    bool linearDirectionSegmentStaysDefined(const Vector3d& directionAtOpen,
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

    bool linearDirectionSegmentStaysOffUpAxis(const Vector3d& directionAtOpen,
                                              const Vector3d& directionAtClose) {
      const Vector3d horizontalAtOpen(directionAtOpen.x(), 0.0, directionAtOpen.z());
      const Vector3d horizontalAtClose(directionAtClose.x(), 0.0, directionAtClose.z());
      const Vector3d horizontalDelta = horizontalAtClose - horizontalAtOpen;
      const double deltaLengthSquared = horizontalDelta * horizontalDelta;
      double closestT = 0.0;
      if (deltaLengthSquared > std::numeric_limits<double>::epsilon()) {
        closestT = std::clamp(-(horizontalAtOpen * horizontalDelta) / deltaLengthSquared, 0.0, 1.0);
      }
      return (horizontalAtOpen + horizontalDelta * closestT).length() >
             std::numeric_limits<double>::epsilon();
    }

    bool hasStableDescriptorBasis(const Matrix4d& openMatrix, const Matrix4d& closeMatrix) {
      const Vector3d openRight =
        openMatrix.transformDirection(Vector3d(1.0, 0.0, 0.0)).normalized();
      const Vector3d closeRight =
        closeMatrix.transformDirection(Vector3d(1.0, 0.0, 0.0)).normalized();
      const Vector3d openDown = openMatrix.transformDirection(Vector3d(0.0, 1.0, 0.0)).normalized();
      const Vector3d closeDown =
        closeMatrix.transformDirection(Vector3d(0.0, 1.0, 0.0)).normalized();
      const Vector3d openForward = openMatrix.transformDirection(Vector3d::forward()).normalized();
      const Vector3d closeForward =
        closeMatrix.transformDirection(Vector3d::forward()).normalized();
      return openRight.isDefined() && closeRight.isDefined() && openDown.isDefined() &&
             closeDown.isDefined() && openForward.isDefined() && closeForward.isDefined() &&
             nearlyEqual(openRight, closeRight) && nearlyEqual(openDown, closeDown) &&
             nearlyEqual(openForward, closeForward);
    }
  }

  Vector3d SampledShutterLookAtDescriptorMotion::positionDelta() const {
    return positionAtClose - positionAtOpen;
  }

  Vector3d SampledShutterLookAtDescriptorMotion::targetDelta() const {
    return targetAtClose - targetAtOpen;
  }

  std::optional<SampledShutterLookAtDescriptorMotion>
  sampledLookAtShutterMotion(const Camera& camera) {
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

    const Vector3d positionAtOpen =
      animatedVectorAt(camera, "position", camera.position(), shutterOpen);
    const Vector3d positionAtClose =
      animatedVectorAt(camera, "position", camera.position(), shutterClose);
    const Vector3d targetAtOpen = animatedVectorAt(camera, "target", camera.target(), shutterOpen);
    const Vector3d targetAtClose =
      animatedVectorAt(camera, "target", camera.target(), shutterClose);
    const Vector3d directionAtOpen = targetAtOpen - positionAtOpen;
    const Vector3d directionAtClose = targetAtClose - positionAtClose;
    if (!hasDefinedDirection(positionAtOpen, targetAtOpen) ||
        !hasDefinedDirection(positionAtClose, targetAtClose) ||
        !linearDirectionSegmentStaysDefined(directionAtOpen, directionAtClose) ||
        !linearDirectionSegmentStaysOffUpAxis(directionAtOpen, directionAtClose)) {
      return std::nullopt;
    }

    return SampledShutterLookAtDescriptorMotion{positionAtOpen, positionAtClose, targetAtOpen,
                                                targetAtClose};
  }

  std::optional<SampledShutterDescriptorMotion>
  sampledStableBasisShutterMotion(const Camera& camera) {
    const std::optional<SampledShutterLookAtDescriptorMotion> motion =
      sampledLookAtShutterMotion(camera);
    if (!motion) {
      return std::nullopt;
    }

    const Matrix4d matrixAtOpen =
      Matrix4d::lookAt(motion->positionAtOpen, motion->targetAtOpen, Vector3d::up());
    const Matrix4d matrixAtClose =
      Matrix4d::lookAt(motion->positionAtClose, motion->targetAtClose, Vector3d::up());
    if (!hasStableDescriptorBasis(matrixAtOpen, matrixAtClose)) {
      return std::nullopt;
    }
    return SampledShutterDescriptorMotion{matrixAtOpen, matrixAtClose};
  }
}
