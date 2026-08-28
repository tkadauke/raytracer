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
  using render::detail::fillGpuDescriptorPointSourceMotion;
  using render::detail::fillGpuDescriptorViewport;
  using render::detail::hasValidGpuPrimaryPathSampler;
  using render::detail::isEmptyGpuPrimaryPathRect;
  using render::detail::pointSourceDescriptorMotion;
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
  return rayFromPointSource(direction(x, y));
}

std::optional<GpuPrimaryPathDescriptor>
FishEyeCamera::gpuPrimaryPathDescriptor(const Recti& rect, std::uint32_t sampleSeed) const {
  auto plane = viewPlane();
  if (!hasValidGpuPrimaryPathSampler(plane)) {
    return std::nullopt;
  }
  if (animationTrack("fieldOfView")) {
    return std::nullopt;
  }
  const auto motion = pointSourceDescriptorMotion(*this, fixedShutterGpuCameraMatrix());
  if (!motion) {
    return std::nullopt;
  }

  const Recti actual = renderableRect(rect);
  if (isEmptyGpuPrimaryPathRect(actual)) {
    return std::nullopt;
  }

  checkGpuPathCount(actual, plane->sampler()->numSamples());

  GpuPrimaryPathDescriptor descriptor;
  descriptor.mode = gpuPrimaryPathGenerationModeFishEye;
  fillGpuDescriptorPointSourceMotion(descriptor.rectilinear, *motion);
  descriptor.rectilinear.lensParameters =
    gpuFloat4(plane->width(), plane->height(), m_fieldOfView.radians());
  fillGpuDescriptorViewport(descriptor.rectilinear, rect, actual, plane->sampler()->numSamples(),
                            sampleSeed);
  return descriptor;
}

static bool dummy = CameraFactory::self().registerClass<FishEyeCamera>("FishEyeCamera");
