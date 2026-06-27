#include "render/cameras/CameraFactory.h"
#include "render/cameras/OrthographicCamera.h"
#include "core/math/Ray.h"
#include "render/samplers/Sampler.h"
#include "render/viewplanes/ViewPlane.h"

#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

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
}

std::shared_ptr<Camera> OrthographicCamera::clone() const {
  auto result = std::make_shared<OrthographicCamera>();
  copyBaseStateTo(*result);
  result->m_zoom = m_zoom;
  return result;
}

const char* OrthographicCamera::fingerprintType() const {
  return "OrthographicCamera";
}

Rayd OrthographicCamera::rayForPixel(double x, double y, render::SampleStream&) const {
  Vector3d direction = matrix().transformDirection(Vector3d::forward());
  Vector3d pixel = viewPlane()->pixelAt(x, y);
  return Rayd(pixel, direction);
}

std::optional<GpuPrimaryPathDescriptor>
OrthographicCamera::gpuPrimaryPathDescriptor(const Recti& rect, std::uint32_t sampleSeed) const {
  auto plane = viewPlane();
  if (!plane || !plane->sampler() || plane->sampler()->numSamples() <= 0) {
    return std::nullopt;
  }
  const std::optional<Matrix4d> descriptorMatrix = fixedShutterGpuCameraMatrix();
  if (!descriptorMatrix) {
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
    throw std::overflow_error("GPU orthographic primary path count overflows");
  }
  (void)checkedU32(pathCount, "GPU orthographic primary path count");

  GpuPrimaryPathDescriptor descriptor;
  descriptor.mode = gpuPrimaryPathGenerationModeOrthographic;
  descriptor.rectilinear.originOrDirection =
    vector4(descriptorMatrix->transformDirection(Vector3d::forward()).normalized(), 0.0f);
  descriptor.rectilinear.topLeft = vector4(plane->pixelAt(0.0, 0.0), 1.0f);
  descriptor.rectilinear.right = vector4(plane->pixelAt(1.0, 0.0) - plane->pixelAt(0.0, 0.0), 0.0f);
  descriptor.rectilinear.down = vector4(plane->pixelAt(0.0, 1.0) - plane->pixelAt(0.0, 0.0), 0.0f);
  descriptor.rectilinear.requestedLeft = rect.left();
  descriptor.rectilinear.requestedTop = rect.top();
  descriptor.rectilinear.requestedWidth =
    checkedU32(static_cast<std::uint64_t>(rect.width()), "GPU orthographic requested width");
  descriptor.rectilinear.requestedHeight =
    checkedU32(static_cast<std::uint64_t>(rect.height()), "GPU orthographic requested height");
  descriptor.rectilinear.actualLeft = actual.left();
  descriptor.rectilinear.actualTop = actual.top();
  descriptor.rectilinear.actualWidth =
    checkedU32(static_cast<std::uint64_t>(actual.width()), "GPU orthographic actual width");
  descriptor.rectilinear.actualHeight =
    checkedU32(static_cast<std::uint64_t>(actual.height()), "GPU orthographic actual height");
  descriptor.rectilinear.samplesPerPixel =
    checkedU32(static_cast<std::uint64_t>(plane->sampler()->numSamples()),
               "GPU orthographic samples per pixel");
  descriptor.rectilinear.sampleSeed = sampleSeed;
  return descriptor;
}

Vector2d OrthographicCamera::projectPoint(const Vector3d& worldPoint) const {
  // Orthographic projection: drop the camera-forward axis component
  // and convert the remaining camera-space (x, y) directly into
  // pixel coordinates. No perspective divide — every world point
  // projects via parallel rays.
  const Matrix4d& worldToCamera = inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  if (pCam.z() < 0.0) {
    return Vector2d::undefined;
  }

  auto plane = viewPlane();
  double pxSize = plane->pixelSize();
  double halfH = plane->hSpan() / 2.0;
  double halfV = plane->vSpan() / 2.0;
  double lx = pCam.x() / pxSize;
  double ly = pCam.y() / pxSize;
  const Recti& inner = plane->innerRect();
  double x = (lx + halfH) * inner.width() / plane->hSpan() + inner.left();
  double y = (ly + halfV) * inner.height() / plane->vSpan() + inner.top();
  return Vector2d(x, y);
}

Vector3d OrthographicCamera::projectPointWithDepth(const Vector3d& worldPoint) const {
  const Matrix4d& worldToCamera = inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  if (pCam.z() < 0.0) {
    return Vector3d::undefined;
  }

  auto plane = viewPlane();
  double pxSize = plane->pixelSize();
  double halfH = plane->hSpan() / 2.0;
  double halfV = plane->vSpan() / 2.0;
  double lx = pCam.x() / pxSize;
  double ly = pCam.y() / pxSize;
  const Recti& inner = plane->innerRect();
  double x = (lx + halfH) * inner.width() / plane->hSpan() + inner.left();
  double y = (ly + halfV) * inner.height() / plane->vSpan() + inner.top();
  // For orthographic projection, depth IS the camera-space z; no
  // m_distance offset since there's no perspective eye point. The
  // rasterizer uses clip.w for projective interpolation, and our
  // clip.w is always 1, so depth and attributes interpolate linearly.
  return Vector3d(x, y, pCam.z());
}

Matrix4d OrthographicCamera::projectionMatrix() const {
  auto plane = viewPlane();
  const double halfW = plane->hSpan() * plane->pixelSize() / 2.0;
  const double halfH = plane->vSpan() * plane->pixelSize() / 2.0;
  return Matrix4d::orthographic(-halfW, halfW, -halfH, halfH, 0.0, 1e6);
}

Vector4d OrthographicCamera::projectPointToClipSpace(const Vector3d& worldPoint) const {
  const Matrix4d& worldToCamera = inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);

  const Vector4d v = projectionMatrix() * Vector4d(pCam.x(), pCam.y(), pCam.z(), 1.0);
  // Use camera-space z for the depth component: the rasterizer's
  // HomogeneousClipVolume depth planes and depth buffer use it directly in
  // eye-space units rather than the NDC z the orthographic factory produces.
  return Vector4d(v.x(), v.y(), pCam.z(), 1.0);
}

double OrthographicCamera::eyeRelativeDepth(const Vector3d& worldPoint) const {
  // Orthographic projection has no perspective eye point; depth is
  // just the camera-space `z` coordinate. Positive in front of the
  // viewplane, negative behind.
  const Matrix4d& worldToCamera = inverseMatrix();
  Vector3d pCam = worldToCamera * Vector4d(worldPoint);
  return pCam.z();
}

void OrthographicCamera::setViewPlane(std::shared_ptr<render::ViewPlane> plane) {
  Camera::setViewPlane(plane);
  viewPlane()->setPixelSize(1.0 / m_zoom);
}

static bool dummy = CameraFactory::self().registerClass<OrthographicCamera>("OrthographicCamera");
