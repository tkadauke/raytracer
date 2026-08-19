#include "render/cameras/CameraFactory.h"
#include "CameraMotionHelpers.h"
#include "GpuPrimaryPathDescriptorPacking.h"
#include "core/DivisionByZeroException.h"
#include "render/cameras/PinholeCamera.h"
#include "PinholeProjection.h"
#include "core/math/Ray.h"
#include "render/samplers/Sampler.h"
#include "render/viewplanes/ViewPlane.h"

#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

using namespace render;

std::shared_ptr<Camera> PinholeCamera::clone() const {
  auto result = std::make_shared<PinholeCamera>();
  copyBaseStateTo(*result);
  result->m_distance = m_distance;
  result->m_zoom = m_zoom;
  return result;
}

const char* PinholeCamera::fingerprintType() const {
  return "PinholeCamera";
}

namespace {
  using render::detail::animatedVectorAt;
  using render::detail::checkedU32;
  using render::detail::checkGpuPathCount;
  using render::detail::eyeOriginForMatrix;
  using render::detail::fillGpuDescriptorPlane;
  using render::detail::fillGpuDescriptorViewport;
  using render::detail::nearlyEqual;
  using render::detail::sampledLinearShutterMotionEndpoints;

  struct DescriptorMotion {
    std::uint32_t motionMode{gpuPrimaryPathMotionModeOriginDelta};
    Vector3d originOrPosition;
    Vector3d motionOriginOrPositionDelta{Vector3d::null};
    Vector3d target{Vector3d::null};
    Vector3d targetDelta{Vector3d::null};
    double distance{0.0};
  };

  std::optional<DescriptorMotion> sampledShutterPinholeMotion(const PinholeCamera& camera,
                                                              double distance) {
    const auto endpoints = sampledLinearShutterMotionEndpoints(camera);
    if (!endpoints) {
      return std::nullopt;
    }

    const Matrix4d matrixAtOpen =
      Matrix4d::lookAt(endpoints->positionAtOpen, endpoints->targetAtOpen, Vector3d::up());
    const Matrix4d matrixAtClose =
      Matrix4d::lookAt(endpoints->positionAtClose, endpoints->targetAtClose, Vector3d::up());
    const Vector3d originAtOpen = eyeOriginForMatrix(matrixAtOpen, distance);
    const Vector3d originAtClose = eyeOriginForMatrix(matrixAtClose, distance);
    if (nearlyEqual(endpoints->directionAtClose, endpoints->directionAtOpen)) {
      return DescriptorMotion{gpuPrimaryPathMotionModeOriginDelta, originAtOpen,
                              originAtClose - originAtOpen};
    }

    return DescriptorMotion{gpuPrimaryPathMotionModeLookAt,
                            endpoints->positionAtOpen,
                            endpoints->positionAtClose - endpoints->positionAtOpen,
                            endpoints->targetAtOpen,
                            endpoints->targetAtClose - endpoints->targetAtOpen,
                            distance};
  }

}

Vector3d PinholeCamera::rayOrigin() const {
  return render::detail::eyeOriginForMatrix(matrix(), m_distance);
}

Rayd PinholeCamera::rayForPixel(double x, double y, render::SampleStream&) const {
  Vector3d position = rayOrigin();
  Vector3d pixel = viewPlane()->pixelAt(x, y);
  return Rayd(position, (pixel - position).normalized());
}

std::unique_ptr<Camera::PrimaryRayGenerator> PinholeCamera::primaryRayGenerator() const {
  class PinholePrimaryRayGenerator final : public Camera::PrimaryRayGenerator {
  public:
    PinholePrimaryRayGenerator(std::shared_ptr<render::ViewPlane> plane,
                               const PinholeCamera& camera, const Vector3d& origin)
        : m_plane(std::move(plane)),
          m_camera(camera),
          m_origin(origin) {
    }

    std::optional<Camera::PrimaryRay> sample(const render::ViewPlane::Iterator& pixel,
                                             render::SampleStream& stream) const override {
      const render::SampleStream::PrimarySample primarySample = stream.primarySample();
      const double animationTime = m_camera.animationTimeForSample(primarySample.time);
      const Vector2d xy = pixel.pixel() + primarySample.pixel;
      const Vector3d pixelPoint = m_plane->pixelAt(xy.x(), xy.y());
      const Vector3d origin = rayOriginAt(animationTime);
      const Rayd ray(origin, (pixelPoint - origin).normalized());
      if (!ray.direction().isDefined()) {
        return std::nullopt;
      }

      return Camera::PrimaryRay{ray, primarySample.time, animationTime};
    }

  private:
    Vector3d rayOriginAt(double time) const {
      if (!m_camera.animationTrack("position") && !m_camera.animationTrack("target")) {
        return m_origin;
      }

      const Vector3d position = animatedVectorAt(m_camera, "position", m_camera.position(), time);
      const Vector3d target = animatedVectorAt(m_camera, "target", m_camera.target(), time);
      const Matrix4d cameraMatrix = Matrix4d::lookAt(position, target, Vector3d::up());
      return eyeOriginForMatrix(cameraMatrix, m_camera.distance());
    }

    std::shared_ptr<render::ViewPlane> m_plane;
    const PinholeCamera& m_camera;
    Vector3d m_origin;
  };

  return std::make_unique<PinholePrimaryRayGenerator>(viewPlane(), *this, rayOrigin());
}

std::optional<GpuPrimaryPathDescriptor>
PinholeCamera::gpuPrimaryPathDescriptor(const Recti& rect, std::uint32_t sampleSeed) const {
  auto plane = viewPlane();
  if (!plane || !plane->sampler() || plane->sampler()->numSamples() <= 0) {
    return std::nullopt;
  }

  std::optional<DescriptorMotion> motion;
  if (const std::optional<Matrix4d> descriptorMatrix = fixedShutterGpuCameraMatrix()) {
    motion = DescriptorMotion{gpuPrimaryPathMotionModeOriginDelta,
                              eyeOriginForMatrix(*descriptorMatrix, m_distance)};
  } else {
    motion = sampledShutterPinholeMotion(*this, m_distance);
  }
  if (!motion) {
    return std::nullopt;
  }

  const Recti actual = renderableRect(rect);
  if (actual.width() <= 0 || actual.height() <= 0) {
    return std::nullopt;
  }

  checkGpuPathCount(actual, plane->sampler()->numSamples());

  GpuPrimaryPathDescriptor descriptor;
  descriptor.mode = gpuPrimaryPathGenerationModePinhole;
  descriptor.rectilinear.motionMode = motion->motionMode;
  descriptor.rectilinear.originOrDirection = gpuFloat4(motion->originOrPosition, 1.0f);
  descriptor.rectilinear.motionOriginDelta = gpuFloat4(motion->motionOriginOrPositionDelta, 0.0f);
  descriptor.rectilinear.motionTarget = gpuFloat4(motion->target, 1.0f);
  descriptor.rectilinear.motionTargetDelta = gpuFloat4(motion->targetDelta, 0.0f);
  descriptor.rectilinear.motionParameters = {static_cast<float>(motion->distance), 0.0f, 0.0f,
                                             0.0f};
  fillGpuDescriptorPlane(descriptor.rectilinear, *plane);
  fillGpuDescriptorViewport(descriptor.rectilinear, rect, actual, plane->sampler()->numSamples(),
                            sampleSeed);
  return descriptor;
}

Vector2d PinholeCamera::projectPoint(const Vector3d& worldPoint) const {
  return detail::PinholeProjection(*this, m_distance).projectPoint(worldPoint);
}

Vector3d PinholeCamera::projectPointWithDepth(const Vector3d& worldPoint) const {
  return detail::PinholeProjection(*this, m_distance).projectPointWithDepth(worldPoint);
}

Matrix4d PinholeCamera::projectionMatrix() const {
  return detail::PinholeProjection(*this, m_distance).projectionMatrix();
}

Vector4d PinholeCamera::projectPointToClipSpace(const Vector3d& worldPoint) const {
  return detail::PinholeProjection(*this, m_distance).projectPointToClipSpace(worldPoint);
}

std::optional<Matrix4d> PinholeCamera::worldToClipMatrix() const {
  // A default-constructed `PinholeCamera` has eye == target, which
  // makes `lookAt` produce a singular matrix; `inverseMatrix()` would
  // then throw `DivisionByZeroException`. Treat that as "no usable
  // matrix" — callers fall back to the per-vertex CPU projection path.
  //
  // The Y axis is negated relative to a textbook view-projection matrix:
  // the project's screen convention places world Y+ at the BOTTOM of
  // the image (see `PinholeProjection::projectPoint`), while a standard
  // GL frustum places it at the top. Without this flip the GPU
  // rasterizer renders mirrored vertically relative to the CPU path.
  static const Matrix4d flipY(1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  try {
    return flipY * projectionMatrix() * Matrix4d::translate(0.0, 0.0, m_distance) * inverseMatrix();
  } catch (const DivisionByZeroException&) {
    return std::nullopt;
  }
}

double PinholeCamera::eyeRelativeDepth(const Vector3d& worldPoint) const {
  return detail::PinholeProjection(*this, m_distance).eyeRelativeDepth(worldPoint);
}

void PinholeCamera::setViewPlane(std::shared_ptr<render::ViewPlane> plane) {
  Camera::setViewPlane(plane);
  viewPlane()->setPixelSize(1.0 / m_zoom);
}

static bool dummy = CameraFactory::self().registerClass<render::PinholeCamera>("PinholeCamera");
