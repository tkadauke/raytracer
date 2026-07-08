#include "render/cameras/CameraFactory.h"
#include "GpuPrimaryPathDescriptorPacking.h"
#include "render/cameras/TiltShiftCamera.h"
#include "core/math/Ray.h"
#include "render/cameras/SampledShutterDescriptorMotion.h"
#include "render/samplers/Sampler.h"
#include "render/viewplanes/ViewPlane.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

using namespace render;

namespace {
  using render::detail::checkedU32;
  using render::detail::checkGpuPathCount;
  using render::detail::fillGpuDescriptorViewport;
  using render::detail::parameters4;
  using render::detail::vector4;

  struct TiltShiftDescriptorMotion {
    std::uint32_t motionMode{gpuPrimaryPathMotionModeOriginDelta};
    Matrix4d matrixAtOpen;
    Vector3d originOrPosition;
    Vector3d motionOriginOrPositionDelta{Vector3d::null};
    Vector3d target{Vector3d::null};
    Vector3d targetDelta{Vector3d::null};

    [[nodiscard]] Matrix4d planeMatrix() const {
      if (motionMode == gpuPrimaryPathMotionModeLookAt) {
        return Matrix4d();
      }
      return matrixAtOpen;
    }
  };

  Vector3d eyeOriginForMatrix(const Matrix4d& matrix, double distance) {
    return matrix.transformPoint(Vector3d(0.0, 0.0, -distance));
  }
}

std::shared_ptr<Camera> TiltShiftCamera::clone() const {
  auto result = std::make_shared<TiltShiftCamera>();
  copyBaseStateTo(*result);
  result->setDistance(distance());
  result->setZoom(zoom());
  result->setApertureRadius(apertureRadius());
  result->setFocalDistance(focalDistance());
  result->m_tilt = m_tilt;
  result->m_shift = m_shift;
  return result;
}

const char* TiltShiftCamera::fingerprintType() const {
  return "TiltShiftCamera";
}

Rayd TiltShiftCamera::rayForPixelWithLens(double x, double y, double lensU, double lensV) const {
  // Pinhole reference ray, with optional lateral shift baked into
  // the principal direction. Conceptually, `setShift({sx, sy})`
  // slides the lens parallel to the sensor by `(sx, sy)` in the
  // camera's right/up basis — which is geometrically equivalent to
  // adding `(sx, sy)` to the target point in the same basis without
  // moving the eye. The projection onto the focal plane below picks
  // up the same offset, so the focal-plane convergence guarantee is
  // preserved.
  Vector3d eyeOrigin = matrix() * Vector4d(0, 0, -distance());
  Vector3d pixelPoint = viewPlane()->pixelAt(x, y);
  Vector3d right = Matrix3d(matrix()) * Vector3d(1, 0, 0);
  Vector3d up = Matrix3d(matrix()) * Vector3d(0, 1, 0);
  Vector3d shiftedPixel = pixelPoint + right * m_shift.x() + up * m_shift.y();
  Vector3d pinholeDir = (shiftedPixel - eyeOrigin).normalized();

  // Tilt the focal-plane normal off the forward axis. Rotation is
  // around the camera's local right axis: positive tilt rotates the
  // top of the focal plane toward the camera (the canonical
  // "miniature" direction). At tilt=0 this collapses to `forward`
  // and the math reduces to ThinLens's perpendicular focal plane.
  Vector3d forward = Matrix3d(matrix()) * Vector3d(0, 0, 1);
  double tiltRad = m_tilt.radians();
  double cosT = std::cos(tiltRad);
  double sinT = std::sin(tiltRad);
  // Rodrigues for rotating `forward` around `right` by tiltRad.
  // Since `forward · right = 0` (orthogonal basis), the formula
  // simplifies to: forward * cosT + (right × forward) * sinT.
  Vector3d tiltedNormal = forward * cosT + (right ^ forward) * sinT;

  // Focal-plane intersection: plane passes through
  //   P0 = eyeOrigin + (distance + focalDistance) * forward
  // (same anchor point as ThinLens — what changes is the *normal*).
  // Ray: eyeOrigin + t * pinholeDir.
  // Solve t = ((P0 - eyeOrigin) · n) / (pinholeDir · n).
  double focalAlongForward = distance() + focalDistance();
  double numerator = focalAlongForward * (forward * tiltedNormal);
  double denom = pinholeDir * tiltedNormal;
  // denom approaches zero when pinholeDir is parallel to the tilted
  // plane (extreme tilt + grazing pixel). We don't handle that here
  // — practical tilts (≤ 60°) keep the denominator well away from
  // zero for typical FOVs.
  double t = numerator / denom;
  Vector3d focalPoint = eyeOrigin + pinholeDir * t;

  // Lens-disc origin offset — same as ThinLens. The displaced ray
  // still has to pass through `focalPoint`, so the focal-plane
  // convergence guarantee carries over: every lens sample for the
  // same pixel converges at the (now tilted) focal point, and
  // points on the tilted focal plane stay sharp.
  Vector3d lensOffset = (right * lensU + up * lensV) * apertureRadius();
  Vector3d lensOrigin = eyeOrigin + lensOffset;

  return Rayd(lensOrigin, (focalPoint - lensOrigin).normalized());
}

std::optional<GpuPrimaryPathDescriptor>
TiltShiftCamera::gpuPrimaryPathDescriptor(const Recti& rect, std::uint32_t sampleSeed) const {
  auto plane = viewPlane();
  if (!plane || !plane->sampler() || plane->sampler()->numSamples() <= 0) {
    return std::nullopt;
  }
  if (animationTrack("distance") || animationTrack("zoom") || animationTrack("apertureRadius") ||
      animationTrack("focalDistance") || animationTrack("tilt") || animationTrack("shift")) {
    return std::nullopt;
  }
  std::optional<TiltShiftDescriptorMotion> motion;
  if (const std::optional<Matrix4d> descriptorMatrix = fixedShutterGpuCameraMatrix()) {
    motion = TiltShiftDescriptorMotion{gpuPrimaryPathMotionModeOriginDelta, *descriptorMatrix,
                                       eyeOriginForMatrix(*descriptorMatrix, distance())};
  } else {
    if (const std::optional<detail::SampledShutterDescriptorMotion> stableMotion =
          detail::sampledStableBasisShutterMotion(*this);
        stableMotion) {
      motion =
        TiltShiftDescriptorMotion{gpuPrimaryPathMotionModeOriginDelta, stableMotion->matrixAtOpen,
                                  eyeOriginForMatrix(stableMotion->matrixAtOpen, distance()),
                                  eyeOriginForMatrix(stableMotion->matrixAtClose, distance()) -
                                    eyeOriginForMatrix(stableMotion->matrixAtOpen, distance())};
    } else if (const std::optional<detail::SampledShutterLookAtDescriptorMotion> lookAtMotion =
                 detail::sampledLookAtShutterMotion(*this);
               lookAtMotion) {
      motion = TiltShiftDescriptorMotion{
        gpuPrimaryPathMotionModeLookAt,
        Matrix4d::lookAt(lookAtMotion->positionAtOpen, lookAtMotion->targetAtOpen, Vector3d::up()),
        lookAtMotion->positionAtOpen,
        lookAtMotion->positionDelta(),
        lookAtMotion->targetAtOpen,
        lookAtMotion->targetDelta()};
    }
  }
  if (!motion) {
    return std::nullopt;
  }

  const Recti actual = renderableRect(rect);
  if (actual.width() <= 0 || actual.height() <= 0) {
    return std::nullopt;
  }

  checkGpuPathCount(actual, plane->sampler()->numSamples());

  const Vector3d forward = motion->matrixAtOpen.transformDirection(Vector3d(0, 0, 1)).normalized();
  const Vector3d right = motion->matrixAtOpen.transformDirection(Vector3d(1, 0, 0));
  const Vector3d up = motion->matrixAtOpen.transformDirection(Vector3d(0, 1, 0));

  auto descriptorPlane = plane->clone();
  descriptorPlane->setup(motion->planeMatrix(), plane->window());

  GpuPrimaryPathDescriptor descriptor;
  descriptor.mode = gpuPrimaryPathGenerationModeTiltShift;
  descriptor.rectilinear.motionMode = motion->motionMode;
  descriptor.rectilinear.originOrDirection = vector4(motion->originOrPosition, 1.0f);
  descriptor.rectilinear.motionOriginDelta = vector4(motion->motionOriginOrPositionDelta, 0.0f);
  descriptor.rectilinear.motionTarget = vector4(motion->target, 1.0f);
  descriptor.rectilinear.motionTargetDelta = vector4(motion->targetDelta, 0.0f);
  descriptor.rectilinear.motionParameters = parameters4(distance(), apertureRadius(), 0.0, 0.0);
  descriptor.rectilinear.topLeft = vector4(descriptorPlane->pixelAt(0.0, 0.0), 1.0f);
  descriptor.rectilinear.right =
    vector4(descriptorPlane->pixelAt(1.0, 0.0) - descriptorPlane->pixelAt(0.0, 0.0), 0.0f);
  descriptor.rectilinear.down =
    vector4(descriptorPlane->pixelAt(0.0, 1.0) - descriptorPlane->pixelAt(0.0, 0.0), 0.0f);
  descriptor.rectilinear.lensRight = vector4(right * apertureRadius(), 0.0f);
  descriptor.rectilinear.lensUp = vector4(up * apertureRadius(), 0.0f);
  descriptor.rectilinear.forward = vector4(forward, 0.0f);
  descriptor.rectilinear.lensParameters =
    parameters4(distance() + focalDistance(), shift().x(), shift().y(), tilt().radians());
  fillGpuDescriptorViewport(descriptor.rectilinear, rect, actual,
                             plane->sampler()->numSamples(), sampleSeed);
  return descriptor;
}

static bool dummy = CameraFactory::self().registerClass<TiltShiftCamera>("TiltShiftCamera");
