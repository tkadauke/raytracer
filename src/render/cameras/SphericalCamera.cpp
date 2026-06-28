#include "render/cameras/CameraFactory.h"
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
  std::uint32_t checkedU32(std::uint64_t value, const char* label) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
      throw std::overflow_error(std::string(label) + " exceeds GPU 32-bit count range");
    }
    return static_cast<std::uint32_t>(value);
  }

  std::array<float, 4> vector4(const Vector3d& value, float w) {
    return {static_cast<float>(value.x()), static_cast<float>(value.y()),
            static_cast<float>(value.z()), w};
  }

  std::array<float, 4> parameters4(double x, double y, double z, double w) {
    return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z),
            static_cast<float>(w)};
  }
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

  const std::uint64_t pixelCount =
    static_cast<std::uint64_t>(actual.width()) * static_cast<std::uint64_t>(actual.height());
  const std::uint64_t pathCount =
    pixelCount * static_cast<std::uint64_t>(plane->sampler()->numSamples());
  if (pixelCount != 0 &&
      pathCount / pixelCount != static_cast<std::uint64_t>(plane->sampler()->numSamples())) {
    throw std::overflow_error("GPU spherical primary path count overflows");
  }
  (void)checkedU32(pathCount, "GPU spherical primary path count");

  GpuPrimaryPathDescriptor descriptor;
  descriptor.mode = gpuPrimaryPathGenerationModeSpherical;
  descriptor.rectilinear.motionMode = motionMode;
  descriptor.rectilinear.originOrDirection = vector4(originOrPosition, 1.0f);
  descriptor.rectilinear.motionOriginDelta = vector4(motionOriginDelta, 0.0f);
  descriptor.rectilinear.motionTarget = vector4(motionTarget, 1.0f);
  descriptor.rectilinear.motionTargetDelta = vector4(motionTargetDelta, 0.0f);
  descriptor.rectilinear.motionParameters = {static_cast<float>(motionOriginOffset), 0.0f, 0.0f,
                                             0.0f};
  descriptor.rectilinear.right =
    vector4(descriptorMatrix->transformDirection(Vector3d(1.0, 0.0, 0.0)), 0.0f);
  descriptor.rectilinear.down =
    vector4(descriptorMatrix->transformDirection(Vector3d(0.0, 1.0, 0.0)), 0.0f);
  descriptor.rectilinear.forward =
    vector4(descriptorMatrix->transformDirection(Vector3d(0.0, 0.0, 1.0)), 0.0f);
  descriptor.rectilinear.lensParameters =
    parameters4(plane->width(), plane->height(), m_horizontalFieldOfView.radians(),
                m_verticalFieldOfView.radians());
  descriptor.rectilinear.requestedLeft = rect.left();
  descriptor.rectilinear.requestedTop = rect.top();
  descriptor.rectilinear.requestedWidth =
    checkedU32(static_cast<std::uint64_t>(rect.width()), "GPU spherical requested width");
  descriptor.rectilinear.requestedHeight =
    checkedU32(static_cast<std::uint64_t>(rect.height()), "GPU spherical requested height");
  descriptor.rectilinear.actualLeft = actual.left();
  descriptor.rectilinear.actualTop = actual.top();
  descriptor.rectilinear.actualWidth =
    checkedU32(static_cast<std::uint64_t>(actual.width()), "GPU spherical actual width");
  descriptor.rectilinear.actualHeight =
    checkedU32(static_cast<std::uint64_t>(actual.height()), "GPU spherical actual height");
  descriptor.rectilinear.samplesPerPixel = checkedU32(
    static_cast<std::uint64_t>(plane->sampler()->numSamples()), "GPU spherical samples per pixel");
  descriptor.rectilinear.sampleSeed = sampleSeed;
  return descriptor;
}

static bool dummy = CameraFactory::self().registerClass<SphericalCamera>("SphericalCamera");
