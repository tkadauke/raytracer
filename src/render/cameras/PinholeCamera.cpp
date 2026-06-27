#include "render/cameras/CameraFactory.h"
#include "core/DivisionByZeroException.h"
#include "render/cameras/PinholeCamera.h"
#include "PinholeProjection.h"
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

  Vector3d rayOriginForMatrix(const Matrix4d& cameraMatrix, double distance) {
    return Vector3d(cameraMatrix.cell(0, 3) - cameraMatrix.cell(0, 2) * distance,
                    cameraMatrix.cell(1, 3) - cameraMatrix.cell(1, 2) * distance,
                    cameraMatrix.cell(2, 3) - cameraMatrix.cell(2, 2) * distance);
  }

  struct DescriptorOrigin {
    Vector3d origin;
    Vector3d motionOriginDelta{Vector3d::null};
  };

  bool isLinearVectorTrack(const render::animation::AnimationTrack* track) {
    return track &&
           track->interpolationMode() == core::math::interpolation::InterpolationMode::Linear &&
           track->valueType() == render::animation::AnimationValue::Type::Vector3;
  }

  bool crossesInteriorKey(const render::animation::AnimationTrack& track, double from, double to) {
    const double start = std::min(from, to);
    const double end = std::max(from, to);
    return std::any_of(
      track.keyframes().begin(), track.keyframes().end(),
      [start, end](const auto& keyframe) { return keyframe.time > start && keyframe.time < end; });
  }

  Vector3d animatedVectorAt(const PinholeCamera& camera, const char* property,
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

  std::optional<DescriptorOrigin> sampledShutterRigTranslationOrigin(const PinholeCamera& camera,
                                                                     double distance) {
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
    if (crossesInteriorKey(*positionTrack, shutterOpen, shutterClose) ||
        crossesInteriorKey(*targetTrack, shutterOpen, shutterClose)) {
      return std::nullopt;
    }

    const Vector3d positionAtOpen =
      animatedVectorAt(camera, "position", camera.position(), shutterOpen);
    const Vector3d positionAtClose =
      animatedVectorAt(camera, "position", camera.position(), shutterClose);
    const Vector3d targetAtOpen = animatedVectorAt(camera, "target", camera.target(), shutterOpen);
    const Vector3d targetAtClose =
      animatedVectorAt(camera, "target", camera.target(), shutterClose);
    const Vector3d baselineDirection = camera.target() - camera.position();
    if (!nearlyEqual(targetAtOpen - positionAtOpen, baselineDirection) ||
        !nearlyEqual(targetAtClose - positionAtClose, baselineDirection)) {
      return std::nullopt;
    }

    const Vector3d originAtOpen =
      rayOriginForMatrix(Matrix4d::lookAt(positionAtOpen, targetAtOpen, Vector3d::up()), distance);
    const Vector3d originAtClose = rayOriginForMatrix(
      Matrix4d::lookAt(positionAtClose, targetAtClose, Vector3d::up()), distance);
    return DescriptorOrigin{originAtOpen, originAtClose - originAtOpen};
  }

}

Vector3d PinholeCamera::rayOrigin() const {
  const Matrix4d& cameraMatrix = matrix();
  return Vector3d(cameraMatrix.cell(0, 3) - cameraMatrix.cell(0, 2) * m_distance,
                  cameraMatrix.cell(1, 3) - cameraMatrix.cell(1, 2) * m_distance,
                  cameraMatrix.cell(2, 3) - cameraMatrix.cell(2, 2) * m_distance);
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
    Vector3d animatedVector(const char* property, const Vector3d& fallback, double time) const {
      const auto* track = m_camera.animationTrack(property);
      if (!track) {
        return fallback;
      }
      return fallback + track->sample(time).get<Vector3d>() -
             track->sample(m_camera.animationFrame()).get<Vector3d>();
    }

    Vector3d rayOriginAt(double time) const {
      if (!m_camera.animationTrack("position") && !m_camera.animationTrack("target")) {
        return m_origin;
      }

      const Vector3d position = animatedVector("position", m_camera.position(), time);
      const Vector3d target = animatedVector("target", m_camera.target(), time);
      const Matrix4d cameraMatrix = Matrix4d::lookAt(position, target, Vector3d::up());
      return Vector3d(cameraMatrix.cell(0, 3) - cameraMatrix.cell(0, 2) * m_camera.distance(),
                      cameraMatrix.cell(1, 3) - cameraMatrix.cell(1, 2) * m_camera.distance(),
                      cameraMatrix.cell(2, 3) - cameraMatrix.cell(2, 2) * m_camera.distance());
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

  std::optional<DescriptorOrigin> origin;
  if (const std::optional<Matrix4d> descriptorMatrix = fixedShutterGpuCameraMatrix()) {
    origin = DescriptorOrigin{rayOriginForMatrix(*descriptorMatrix, m_distance), Vector3d::null};
  } else {
    origin = sampledShutterRigTranslationOrigin(*this, m_distance);
  }
  if (!origin) {
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
    throw std::overflow_error("GPU pinhole primary path count overflows");
  }
  (void)checkedU32(pathCount, "GPU pinhole primary path count");

  GpuPrimaryPathDescriptor descriptor;
  descriptor.mode = gpuPrimaryPathGenerationModePinhole;
  descriptor.rectilinear.originOrDirection = vector4(origin->origin, 1.0f);
  descriptor.rectilinear.motionOriginDelta = vector4(origin->motionOriginDelta, 0.0f);
  descriptor.rectilinear.topLeft = vector4(plane->pixelAt(0.0, 0.0), 1.0f);
  descriptor.rectilinear.right = vector4(plane->pixelAt(1.0, 0.0) - plane->pixelAt(0.0, 0.0), 0.0f);
  descriptor.rectilinear.down = vector4(plane->pixelAt(0.0, 1.0) - plane->pixelAt(0.0, 0.0), 0.0f);
  descriptor.rectilinear.requestedLeft = rect.left();
  descriptor.rectilinear.requestedTop = rect.top();
  descriptor.rectilinear.requestedWidth =
    checkedU32(static_cast<std::uint64_t>(rect.width()), "GPU pinhole requested width");
  descriptor.rectilinear.requestedHeight =
    checkedU32(static_cast<std::uint64_t>(rect.height()), "GPU pinhole requested height");
  descriptor.rectilinear.actualLeft = actual.left();
  descriptor.rectilinear.actualTop = actual.top();
  descriptor.rectilinear.actualWidth =
    checkedU32(static_cast<std::uint64_t>(actual.width()), "GPU pinhole actual width");
  descriptor.rectilinear.actualHeight =
    checkedU32(static_cast<std::uint64_t>(actual.height()), "GPU pinhole actual height");
  descriptor.rectilinear.samplesPerPixel = checkedU32(
    static_cast<std::uint64_t>(plane->sampler()->numSamples()), "GPU pinhole samples per pixel");
  descriptor.rectilinear.sampleSeed = sampleSeed;
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
