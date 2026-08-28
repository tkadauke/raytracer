#include "render/cameras/CameraFactory.h"
#include "CameraMotionHelpers.h"
#include "GpuPrimaryPathDescriptorPacking.h"
#include "render/cameras/OrthographicCamera.h"
#include "core/math/Ray.h"
#include "render/samplers/Sampler.h"
#include "render/viewplanes/ViewPlane.h"

#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

using namespace render;

namespace {
  using render::detail::animatedVectorAt;
  using render::detail::checkedU32;
  using render::detail::checkGpuPathCount;
  using render::detail::fillGpuDescriptorPlane;
  using render::detail::fillGpuDescriptorViewport;
  using render::detail::hasStableBasis;
  using render::detail::hasValidGpuPrimaryPathSampler;
  using render::detail::isEmptyGpuPrimaryPathRect;
  using render::detail::LensDescriptorMotion;
  using render::detail::linearDirectionSegmentStaysOffUpAxis;
  using render::detail::nearlyEqual;
  using render::detail::sampledLinearShutterMotionEndpoints;

  std::optional<LensDescriptorMotion>
  sampledShutterOrthographicMotion(const OrthographicCamera& camera) {
    const auto endpoints = sampledLinearShutterMotionEndpoints(camera);
    if (!endpoints ||
        !linearDirectionSegmentStaysOffUpAxis(endpoints->directionAtOpen,
                                              endpoints->directionAtClose)) {
      return std::nullopt;
    }

    const Matrix4d matrixAtOpen =
      Matrix4d::lookAt(endpoints->positionAtOpen, endpoints->targetAtOpen, Vector3d::up());
    const Matrix4d matrixAtClose =
      Matrix4d::lookAt(endpoints->positionAtClose, endpoints->targetAtClose, Vector3d::up());
    if (hasStableBasis(matrixAtOpen, matrixAtClose)) {
      return LensDescriptorMotion{gpuPrimaryPathMotionModeOriginDelta, matrixAtOpen,
                                  matrixAtOpen.transformDirection(Vector3d::forward()).normalized(),
                                  matrixAtClose.translationVector() -
                                    matrixAtOpen.translationVector()};
    }

    return LensDescriptorMotion{gpuPrimaryPathMotionModeLookAt,
                                matrixAtOpen,
                                endpoints->positionAtOpen,
                                endpoints->positionAtClose - endpoints->positionAtOpen,
                                endpoints->targetAtOpen,
                                endpoints->targetAtClose - endpoints->targetAtOpen};
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
  if (!hasValidGpuPrimaryPathSampler(plane)) {
    return std::nullopt;
  }
  std::optional<LensDescriptorMotion> motion;
  if (const std::optional<Matrix4d> descriptorMatrix = fixedShutterGpuCameraMatrix()) {
    motion =
      LensDescriptorMotion{gpuPrimaryPathMotionModeOriginDelta, *descriptorMatrix,
                           descriptorMatrix->transformDirection(Vector3d::forward()).normalized()};
  } else {
    motion = sampledShutterOrthographicMotion(*this);
  }
  if (!motion) {
    return std::nullopt;
  }

  const Recti actual = renderableRect(rect);
  if (isEmptyGpuPrimaryPathRect(actual)) {
    return std::nullopt;
  }

  checkGpuPathCount(actual, plane->sampler()->numSamples());

  auto descriptorPlane = plane->clone();
  descriptorPlane->setup(motion->planeMatrix(), plane->window());
  GpuPrimaryPathDescriptor descriptor;
  descriptor.mode = gpuPrimaryPathGenerationModeOrthographic;
  descriptor.rectilinear.motionMode = motion->motionMode;
  descriptor.rectilinear.originOrDirection = gpuFloat4(
    motion->originOrPosition, motion->motionMode == gpuPrimaryPathMotionModeLookAt ? 1.0f : 0.0f);
  descriptor.rectilinear.motionOriginDelta = gpuFloat4(motion->motionOriginOrPositionDelta, 0.0f);
  descriptor.rectilinear.motionTarget = gpuFloat4(motion->target, 1.0f);
  descriptor.rectilinear.motionTargetDelta = gpuFloat4(motion->targetDelta, 0.0f);
  fillGpuDescriptorPlane(descriptor.rectilinear, *descriptorPlane);
  fillGpuDescriptorViewport(descriptor.rectilinear, rect, actual, plane->sampler()->numSamples(),
                            sampleSeed);
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
