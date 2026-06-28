#include "render/cameras/CameraFactory.h"
#include "render/cameras/OrthographicCamera.h"
#include "core/math/Ray.h"
#include "render/animation/AnimationTrack.h"
#include "render/samplers/Sampler.h"
#include "render/viewplanes/ViewPlane.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

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

  bool isLinearVectorTrack(const render::animation::AnimationTrack* track) {
    return !track ||
           (track->interpolationMode() == core::math::interpolation::InterpolationMode::Linear &&
            track->valueType() == render::animation::AnimationValue::Type::Vector3);
  }

  bool crossesInteriorKey(const render::animation::AnimationTrack& track, double from, double to) {
    const double start = std::min(from, to);
    const double end = std::max(from, to);
    return std::any_of(
      track.keyframes().begin(), track.keyframes().end(),
      [start, end](const auto& keyframe) { return keyframe.time > start && keyframe.time < end; });
  }

  Vector3d animatedVectorAt(const OrthographicCamera& camera, const char* property,
                            const Vector3d& fallback, double time) {
    const auto* track = camera.animationTrack(property);
    if (!track) {
      return fallback;
    }
    return fallback + track->sample(time).get<Vector3d>() -
           track->sample(camera.animationFrame()).get<Vector3d>();
  }

  bool nearlyEqual(const Vector3d& left, const Vector3d& right) {
    return (left - right).length() <= 1e-9;
  }

  bool hasDefinedDirection(const Vector3d& position, const Vector3d& target) {
    return (target - position).length() > std::numeric_limits<double>::epsilon();
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

  bool hasStableOrthographicBasis(const Matrix4d& openMatrix, const Matrix4d& closeMatrix) {
    const Vector3d openRight = openMatrix.transformDirection(Vector3d(1.0, 0.0, 0.0)).normalized();
    const Vector3d closeRight =
      closeMatrix.transformDirection(Vector3d(1.0, 0.0, 0.0)).normalized();
    const Vector3d openDown = openMatrix.transformDirection(Vector3d(0.0, 1.0, 0.0)).normalized();
    const Vector3d closeDown = closeMatrix.transformDirection(Vector3d(0.0, 1.0, 0.0)).normalized();
    const Vector3d openForward = openMatrix.transformDirection(Vector3d::forward()).normalized();
    const Vector3d closeForward = closeMatrix.transformDirection(Vector3d::forward()).normalized();
    return openRight.isDefined() && closeRight.isDefined() && openDown.isDefined() &&
           closeDown.isDefined() && openForward.isDefined() && closeForward.isDefined() &&
           nearlyEqual(openRight, closeRight) && nearlyEqual(openDown, closeDown) &&
           nearlyEqual(openForward, closeForward);
  }

  struct OrthographicDescriptorMotion {
    std::uint32_t motionMode{gpuPrimaryPathMotionModeOriginDelta};
    Matrix4d matrixAtOpen;
    Vector3d originOrDirection;
    Vector3d motionOriginDelta{Vector3d::null};
    Vector3d target{Vector3d::null};
    Vector3d targetDelta{Vector3d::null};

    [[nodiscard]] Matrix4d planeMatrix() const {
      if (motionMode == gpuPrimaryPathMotionModeLookAt) {
        return Matrix4d();
      }
      return matrixAtOpen;
    }
  };

  std::optional<OrthographicDescriptorMotion>
  sampledShutterOrthographicMotion(const OrthographicCamera& camera) {
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

    const Matrix4d matrixAtOpen = Matrix4d::lookAt(positionAtOpen, targetAtOpen, Vector3d::up());
    const Matrix4d matrixAtClose = Matrix4d::lookAt(positionAtClose, targetAtClose, Vector3d::up());
    if (hasStableOrthographicBasis(matrixAtOpen, matrixAtClose)) {
      return OrthographicDescriptorMotion{
        gpuPrimaryPathMotionModeOriginDelta, matrixAtOpen,
        matrixAtOpen.transformDirection(Vector3d::forward()).normalized(),
        matrixAtClose.translationVector() - matrixAtOpen.translationVector()};
    }

    return OrthographicDescriptorMotion{
      gpuPrimaryPathMotionModeLookAt,   matrixAtOpen, positionAtOpen,
      positionAtClose - positionAtOpen, targetAtOpen, targetAtClose - targetAtOpen};
  }

  Matrix4d animatedMatrixAt(const OrthographicCamera& camera, double time) {
    const Vector3d position = animatedVectorAt(camera, "position", camera.position(), time);
    const Vector3d target = animatedVectorAt(camera, "target", camera.target(), time);
    return Matrix4d::lookAt(position, target, Vector3d::up());
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

std::unique_ptr<Camera::PrimaryRayGenerator> OrthographicCamera::primaryRayGenerator() const {
  class OrthographicPrimaryRayGenerator final : public Camera::PrimaryRayGenerator {
  public:
    OrthographicPrimaryRayGenerator(std::shared_ptr<render::ViewPlane> plane,
                                    const OrthographicCamera& camera)
        : m_plane(std::move(plane)),
          m_camera(camera),
          m_matrix(camera.matrix()),
          m_hasAnimatedPose(camera.animationTrack("position") || camera.animationTrack("target")) {
    }

    std::optional<Camera::PrimaryRay> sample(const render::ViewPlane::Iterator& pixel,
                                             render::SampleStream& stream) const override {
      const render::SampleStream::PrimarySample primarySample = stream.primarySample();
      const double animationTime = m_camera.animationTimeForSample(primarySample.time);
      const Vector2d xy = pixel.pixel() + primarySample.pixel;
      const Matrix4d cameraMatrix = matrixAt(animationTime);
      const std::shared_ptr<render::ViewPlane> plane = planeAt(cameraMatrix);
      if (!plane) {
        return std::nullopt;
      }
      const Vector3d pixelPoint = plane->pixelAt(xy.x(), xy.y());
      const Rayd ray(pixelPoint, cameraMatrix.transformDirection(Vector3d::forward()).normalized());
      if (!ray.direction().isDefined()) {
        return std::nullopt;
      }

      return Camera::PrimaryRay{ray, primarySample.time, animationTime};
    }

  private:
    Matrix4d matrixAt(double time) const {
      if (!m_hasAnimatedPose) {
        return m_matrix;
      }
      return animatedMatrixAt(m_camera, time);
    }

    std::shared_ptr<render::ViewPlane> planeAt(const Matrix4d& matrix) const {
      if (!m_hasAnimatedPose || !m_plane || m_plane->width() <= 0 || m_plane->height() <= 0) {
        return m_plane;
      }
      auto plane = m_plane->clone();
      plane->setup(matrix, m_plane->window());
      return plane;
    }

    std::shared_ptr<render::ViewPlane> m_plane;
    const OrthographicCamera& m_camera;
    Matrix4d m_matrix;
    bool m_hasAnimatedPose{false};
  };

  return std::make_unique<OrthographicPrimaryRayGenerator>(viewPlane(), *this);
}

std::optional<GpuPrimaryPathDescriptor>
OrthographicCamera::gpuPrimaryPathDescriptor(const Recti& rect, std::uint32_t sampleSeed) const {
  auto plane = viewPlane();
  if (!plane || !plane->sampler() || plane->sampler()->numSamples() <= 0) {
    return std::nullopt;
  }
  std::optional<OrthographicDescriptorMotion> motion;
  if (const std::optional<Matrix4d> descriptorMatrix = fixedShutterGpuCameraMatrix()) {
    motion = OrthographicDescriptorMotion{
      gpuPrimaryPathMotionModeOriginDelta, *descriptorMatrix,
      descriptorMatrix->transformDirection(Vector3d::forward()).normalized()};
  } else {
    motion = sampledShutterOrthographicMotion(*this);
  }
  if (!motion) {
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

  auto descriptorPlane = plane->clone();
  descriptorPlane->setup(motion->planeMatrix(), plane->window());
  GpuPrimaryPathDescriptor descriptor;
  descriptor.mode = gpuPrimaryPathGenerationModeOrthographic;
  descriptor.rectilinear.motionMode = motion->motionMode;
  descriptor.rectilinear.originOrDirection = vector4(
    motion->originOrDirection, motion->motionMode == gpuPrimaryPathMotionModeLookAt ? 1.0f : 0.0f);
  descriptor.rectilinear.motionOriginDelta = vector4(motion->motionOriginDelta, 0.0f);
  descriptor.rectilinear.motionTarget = vector4(motion->target, 1.0f);
  descriptor.rectilinear.motionTargetDelta = vector4(motion->targetDelta, 0.0f);
  descriptor.rectilinear.topLeft = vector4(descriptorPlane->pixelAt(0.0, 0.0), 1.0f);
  descriptor.rectilinear.right =
    vector4(descriptorPlane->pixelAt(1.0, 0.0) - descriptorPlane->pixelAt(0.0, 0.0), 0.0f);
  descriptor.rectilinear.down =
    vector4(descriptorPlane->pixelAt(0.0, 1.0) - descriptorPlane->pixelAt(0.0, 0.0), 0.0f);
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
