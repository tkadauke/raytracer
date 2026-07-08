#include "render/cameras/CameraFactory.h"
#include "GpuPrimaryPathDescriptorPacking.h"
#include "render/cameras/EquirectangularCamera.h"
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
  using render::detail::fillGpuDescriptorViewport;
  using render::detail::parameters4;
  using render::detail::vector4;
}

std::shared_ptr<Camera> EquirectangularCamera::clone() const {
  auto result = std::make_shared<EquirectangularCamera>();
  copyBaseStateTo(*result);
  return result;
}

const char* EquirectangularCamera::fingerprintType() const {
  return "EquirectangularCamera";
}

Vector3d EquirectangularCamera::direction(double x, double y) const {
  // Map pixel (x, y) → (lon, lat) in the canonical equirectangular layout:
  //   x = 0       → lon = -π   (left edge,    behind camera)
  //   x = width   → lon = +π   (right edge,   behind camera; image wraps)
  //   y = 0       → lat = +π/2 (top edge,     north pole = "up" direction)
  //   y = height  → lat = -π/2 (bottom edge,  south pole = "down" direction)
  double lon = (2.0 * x / viewPlane()->width() - 1.0) * PI;
  double lat = (1.0 - 2.0 * y / viewPlane()->height()) * (PI / 2.0);

  // Standard sphere-to-cartesian, with one twist for this codebase's
  // axis convention: `Vector3d::up()` returns (0, -1, 0), so "up in
  // the world" is **negative** y. The latitude → y component therefore
  // gets a sign flip — at lat = +π/2 (top of image, the "north pole")
  // we want the ray pointing world-up, which is y = -1, not +1.
  // Without this flip the rendered panorama comes out upside-down (floor
  // at the top, sky at the bottom) because the projection is otherwise
  // mathematically standard.
  double cosLat = cos(lat);
  Vector3d local(cosLat * sin(lon), -sin(lat), cosLat * cos(lon));

  return matrix().transformDirection(local);
}

Rayd EquirectangularCamera::rayForPixel(double x, double y, render::SampleStream&) const {
  Vector3d position = matrix().translationVector();
  return Rayd(position, direction(x, y));
}

std::optional<GpuPrimaryPathDescriptor>
EquirectangularCamera::gpuPrimaryPathDescriptor(const Recti& rect, std::uint32_t sampleSeed) const {
  auto plane = viewPlane();
  if (!plane || !plane->sampler() || plane->sampler()->numSamples() <= 0) {
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
  descriptor.mode = gpuPrimaryPathGenerationModeEquirectangular;
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
  descriptor.rectilinear.lensParameters = parameters4(plane->width(), plane->height());
  fillGpuDescriptorViewport(descriptor.rectilinear, rect, actual,
                             plane->sampler()->numSamples(), sampleSeed);
  return descriptor;
}

static bool dummy =
  CameraFactory::self().registerClass<EquirectangularCamera>("EquirectangularCamera");
