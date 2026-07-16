#include "render/cameras/CameraFactory.h"
#include "GpuPrimaryPathDescriptorPacking.h"
#include "render/cameras/SphericalCamera.h"
#include "core/math/Ray.h"
#include "core/math/Constants.h"
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
  using render::detail::fillGpuDescriptorMatrixBasis;
  using render::detail::fillGpuDescriptorViewport;
  using render::detail::parameters4;
  using render::detail::vector4;
}

std::shared_ptr<Camera> SphericalCamera::clone() const {
  auto result = std::make_shared<SphericalCamera>();
  copyBaseStateTo(*result);
  result->m_horizontalFieldOfView = m_horizontalFieldOfView;
  result->m_verticalFieldOfView = m_verticalFieldOfView;
  return result;
}

const char* SphericalCamera::fingerprintType() const {
  return "SphericalCamera";
}

Vector3d SphericalCamera::direction(double x, double y) const {
  Vector2d point(2.0 / viewPlane()->width() * x + 1.0, 2.0 / viewPlane()->height() * y - 1.0);

  double lambda = point.x() * 0.5 * m_horizontalFieldOfView.radians();
  double psi = point.y() * 0.5 * m_verticalFieldOfView.radians();
  double phi = PI - lambda;
  double theta = 0.5 * PI - psi;
  double sinPhi = sin(phi);
  double cosPhi = cos(phi);
  double sinTheta = sin(theta);
  double cosTheta = cos(theta);
  return matrix().transformDirection(Vector3d(sinTheta * sinPhi, cosTheta, sinTheta * cosPhi));
}

Rayd SphericalCamera::rayForPixel(double x, double y, render::SampleStream&) const {
  Vector3d position = matrix().transformPoint(Vector3d(0, 0, -5));
  return Rayd(position, direction(x, y));
}

std::optional<GpuPrimaryPathDescriptor>
SphericalCamera::gpuPrimaryPathDescriptor(const Recti& rect, std::uint32_t sampleSeed) const {
  auto plane = viewPlane();
  if (!plane || !plane->sampler() || plane->sampler()->numSamples() <= 0) {
    return std::nullopt;
  }
  if (animationTrack("horizontalFieldOfView") || animationTrack("verticalFieldOfView")) {
    return std::nullopt;
  }
  std::optional<Matrix4d> descriptorMatrix = fixedShutterGpuCameraMatrix();
  std::uint32_t motionMode = gpuPrimaryPathMotionModeOriginDelta;
  Vector3d originOrPosition;
  Vector3d motionOriginDelta = Vector3d::null;
  Vector3d motionTarget = Vector3d::null;
  Vector3d motionTargetDelta = Vector3d::null;
  double motionOriginOffset = 0.0;
  if (!descriptorMatrix) {
    const std::optional<detail::SampledShutterDescriptorMotion> motion =
      detail::sampledStableBasisShutterMotion(*this);
    if (motion) {
      descriptorMatrix = motion->matrixAtOpen;
      originOrPosition = motion->matrixAtOpen.transformPoint(Vector3d(0.0, 0.0, -5.0));
      motionOriginDelta = motion->matrixAtClose.transformPoint(Vector3d(0.0, 0.0, -5.0)) -
                          motion->matrixAtOpen.transformPoint(Vector3d(0.0, 0.0, -5.0));
    } else {
      const std::optional<detail::SampledShutterLookAtDescriptorMotion> lookAtMotion =
        detail::sampledLookAtShutterMotion(*this);
      if (!lookAtMotion) {
        return std::nullopt;
      }
      descriptorMatrix =
        Matrix4d::lookAt(lookAtMotion->positionAtOpen, lookAtMotion->targetAtOpen, Vector3d::up());
      motionMode = gpuPrimaryPathMotionModeLookAt;
      originOrPosition = lookAtMotion->positionAtOpen;
      motionOriginDelta = lookAtMotion->positionDelta();
      motionTarget = lookAtMotion->targetAtOpen;
      motionTargetDelta = lookAtMotion->targetDelta();
      motionOriginOffset = 5.0;
    }
  } else {
    originOrPosition = descriptorMatrix->transformPoint(Vector3d(0.0, 0.0, -5.0));
  }

  const Recti actual = renderableRect(rect);
  if (actual.width() <= 0 || actual.height() <= 0) {
    return std::nullopt;
  }

  checkGpuPathCount(actual, plane->sampler()->numSamples());

  GpuPrimaryPathDescriptor descriptor;
  descriptor.mode = gpuPrimaryPathGenerationModeSpherical;
  descriptor.rectilinear.motionMode = motionMode;
  descriptor.rectilinear.originOrDirection = vector4(originOrPosition, 1.0f);
  descriptor.rectilinear.motionOriginDelta = vector4(motionOriginDelta, 0.0f);
  descriptor.rectilinear.motionTarget = vector4(motionTarget, 1.0f);
  descriptor.rectilinear.motionTargetDelta = vector4(motionTargetDelta, 0.0f);
  descriptor.rectilinear.motionParameters = {static_cast<float>(motionOriginOffset), 0.0f, 0.0f,
                                             0.0f};
  fillGpuDescriptorMatrixBasis(descriptor.rectilinear, *descriptorMatrix);
  descriptor.rectilinear.lensParameters =
    parameters4(plane->width(), plane->height(), m_horizontalFieldOfView.radians(),
                m_verticalFieldOfView.radians());
  fillGpuDescriptorViewport(descriptor.rectilinear, rect, actual,
                             plane->sampler()->numSamples(), sampleSeed);
  return descriptor;
}

static bool dummy = CameraFactory::self().registerClass<SphericalCamera>("SphericalCamera");
