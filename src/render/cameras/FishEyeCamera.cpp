#include "render/cameras/CameraFactory.h"
#include "render/cameras/FishEyeCamera.h"
#include "core/math/Ray.h"
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

  std::array<float, 4> parameters4(double x, double y, double z) {
    return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 0.0f};
  }
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
  if (animationTrack("position") || animationTrack("target") || animationTrack("fieldOfView")) {
    return std::nullopt;
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
    throw std::overflow_error("GPU fish-eye primary path count overflows");
  }
  (void)checkedU32(pathCount, "GPU fish-eye primary path count");

  const Matrix4d& cameraMatrix = matrix();

  GpuPrimaryPathDescriptor descriptor;
  descriptor.mode = gpuPrimaryPathGenerationModeFishEye;
  descriptor.rectilinear.originOrDirection = vector4(cameraMatrix.translationVector(), 1.0f);
  descriptor.rectilinear.right =
    vector4(cameraMatrix.transformDirection(Vector3d(1.0, 0.0, 0.0)), 0.0f);
  descriptor.rectilinear.down =
    vector4(cameraMatrix.transformDirection(Vector3d(0.0, 1.0, 0.0)), 0.0f);
  descriptor.rectilinear.forward =
    vector4(cameraMatrix.transformDirection(Vector3d(0.0, 0.0, 1.0)), 0.0f);
  descriptor.rectilinear.lensParameters =
    parameters4(plane->width(), plane->height(), m_fieldOfView.radians());
  descriptor.rectilinear.requestedLeft = rect.left();
  descriptor.rectilinear.requestedTop = rect.top();
  descriptor.rectilinear.requestedWidth =
    checkedU32(static_cast<std::uint64_t>(rect.width()), "GPU fish-eye requested width");
  descriptor.rectilinear.requestedHeight =
    checkedU32(static_cast<std::uint64_t>(rect.height()), "GPU fish-eye requested height");
  descriptor.rectilinear.actualLeft = actual.left();
  descriptor.rectilinear.actualTop = actual.top();
  descriptor.rectilinear.actualWidth =
    checkedU32(static_cast<std::uint64_t>(actual.width()), "GPU fish-eye actual width");
  descriptor.rectilinear.actualHeight =
    checkedU32(static_cast<std::uint64_t>(actual.height()), "GPU fish-eye actual height");
  descriptor.rectilinear.samplesPerPixel = checkedU32(
    static_cast<std::uint64_t>(plane->sampler()->numSamples()), "GPU fish-eye samples per pixel");
  descriptor.rectilinear.sampleSeed = sampleSeed;
  return descriptor;
}

static bool dummy = CameraFactory::self().registerClass<FishEyeCamera>("FishEyeCamera");
