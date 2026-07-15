#include "render/cameras/SampledShutterDescriptorMotion.h"
#include "CameraMotionHelpers.h"

namespace render::detail {

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
    if (!hasStableBasis(matrixAtOpen, matrixAtClose)) {
      return std::nullopt;
    }
    return SampledShutterDescriptorMotion{matrixAtOpen, matrixAtClose};
  }
}
