#include "render/cameras/CameraFactory.h"
#include "GpuPrimaryPathDescriptorPacking.h"
#include "render/cameras/FishEyeCamera.h"
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

using namespace std;
using namespace render;

namespace {
  using render::detail::checkedU32;
  using render::detail::checkGpuPathCount;
  using render::detail::fillGpuDescriptorViewport;
  using render::detail::parameters4;
  using render::detail::vector4;
}

std::shared_ptr<Camera> FishEyeCamera::clone() const {
  auto result = std::make_shared<FishEyeCamera>();
  copyBaseStateTo(*result);
  result->m_fieldOfView = m_fieldOfView;
  return result;
}

const char* FishEyeCamera::fingerprintType() const {
  return "FishEyeCamera";
}

Vector3d FishEyeCamera::direction(double x, double y) const {
  Vector2d point(2.0 / viewPlane()->width() * x - 1.0, 2.0 / viewPlane()->height() * y - 1.0);
  double r2 = point * point;
  if (r2 <= 1.0) {
    double r = sqrt(r2);
    double psi = r * m_fieldOfView.radians() / 2;
    double sinPsi = sin(psi);
    double cosPsi = cos(psi);
    double sinAlpha = point.y() / r;
    double cosAlpha = point.x() / r;
    return matrix().transformDirection(Vector3d(sinPsi * cosAlpha, sinPsi * sinAlpha, cosPsi));
  } else
    return Vector3d::undefined;
}

Rayd FishEyeCamera::rayForPixel(double x, double y, render::SampleStream&) const {
  Vector3d position = matrix().translationVector();
  return Rayd(position, direction(x, y));
}

std::optional<GpuPrimaryPathDescriptor>
FishEyeCamera::gpuPrimaryPathDescriptor(const Recti& rect, std::uint32_t sampleSeed) const {
  auto plane = viewPlane();
  if (!plane || !plane->sampler() || plane->sampler()->numSamples() <= 0) {
    return std::nullopt;
  }
  if (animationTrack("fieldOfView")) {
    return std::nullopt;
  }
  std::optional<Matrix4d> descriptorMatrix = fixedShutterGpuCameraMatrix();
  std::uint32_t motionMode = gpuPrimaryPathMotionModeOriginDelta;
  Vector3d motionOriginDelta = Vector3d::null;
  Vector3d motionTarget = Vector3d::null;
  Vector3d motionTargetDelta = Vector3d::null;
  if (!descriptorMatrix) {
    const std::optional<detail::SampledShutterDescriptorMotion> motion =
      detail::sampledStableBasisShutterMotion(*this);
    if (motion) {
      descriptorMatrix = motion->matrixAtOpen;
      motionOriginDelta =
        motion->matrixAtClose.translationVector() - motion->matrixAtOpen.translationVector();
    } else {
      const std::optional<detail::SampledShutterLookAtDescriptorMotion> lookAtMotion =
        detail::sampledLookAtShutterMotion(*this);
      if (!lookAtMotion) {
        return std::nullopt;
      }
      descriptorMatrix =
        Matrix4d::lookAt(lookAtMotion->positionAtOpen, lookAtMotion->targetAtOpen, Vector3d::up());
      motionMode = gpuPrimaryPathMotionModeLookAt;
      motionOriginDelta = lookAtMotion->positionDelta();
      motionTarget = lookAtMotion->targetAtOpen;
      motionTargetDelta = lookAtMotion->targetDelta();
    }
  }

  const Recti actual = renderableRect(rect);
  if (actual.width() <= 0 || actual.height() <= 0) {
    return std::nullopt;
  }

  checkGpuPathCount(actual, plane->sampler()->numSamples());

  GpuPrimaryPathDescriptor descriptor;
  descriptor.mode = gpuPrimaryPathGenerationModeFishEye;
  descriptor.rectilinear.motionMode = motionMode;
  descriptor.rectilinear.originOrDirection = vector4(descriptorMatrix->translationVector(), 1.0f);
  descriptor.rectilinear.motionOriginDelta = vector4(motionOriginDelta, 0.0f);
  descriptor.rectilinear.motionTarget = vector4(motionTarget, 1.0f);
  descriptor.rectilinear.motionTargetDelta = vector4(motionTargetDelta, 0.0f);
  descriptor.rectilinear.right =
    vector4(descriptorMatrix->transformDirection(Vector3d(1.0, 0.0, 0.0)), 0.0f);
  descriptor.rectilinear.down =
    vector4(descriptorMatrix->transformDirection(Vector3d(0.0, 1.0, 0.0)), 0.0f);
  descriptor.rectilinear.forward =
    vector4(descriptorMatrix->transformDirection(Vector3d(0.0, 0.0, 1.0)), 0.0f);
  descriptor.rectilinear.lensParameters =
    parameters4(plane->width(), plane->height(), m_fieldOfView.radians());
  fillGpuDescriptorViewport(descriptor.rectilinear, rect, actual,
                             plane->sampler()->numSamples(), sampleSeed);
  return descriptor;
}

static bool dummy = CameraFactory::self().registerClass<FishEyeCamera>("FishEyeCamera");
