#include "render/cameras/CameraFactory.h"
#include "GpuPrimaryPathDescriptorPacking.h"
#include "render/cameras/ThinLensCamera.h"
#include "PinholeProjection.h"
#include "core/math/Ray.h"
#include "render/cameras/SampledShutterDescriptorMotion.h"
#include "render/samplers/JitteredSampler.h"
#include "render/samplers/Sampler.h"
#include "render/viewplanes/ViewPlane.h"
#include "render/ConcentricMap.h"

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

using namespace render;

namespace {
  using render::detail::checkedU32;
  using render::detail::checkGpuPathCount;
  using render::detail::concentricMapToDisc;
  using render::detail::eyeOriginForMatrix;
  using render::detail::fillGpuDescriptorViewport;
  using render::detail::parameters4;
  using render::detail::vector4;

  struct ThinLensDescriptorMotion {
    std::uint32_t motionMode{gpuPrimaryPathMotionModeOriginDelta};
    Matrix4d matrixAtOpen;
    Vector3d originOrPosition;
    Vector3d motionOriginOrPositionDelta{Vector3d::null};
    Vector3d target{Vector3d::null};
    Vector3d targetDelta{Vector3d::null};

    [[nodiscard]] Matrix4d planeMatrix() const {
      if (motionMode == gpuPrimaryPathMotionModeLookAt) {
        return Matrix4d();
      }
      return matrixAtOpen;
    }
  };

}

std::shared_ptr<Camera> ThinLensCamera::clone() const {
  auto result = std::make_shared<ThinLensCamera>();
  copyBaseStateTo(*result);
  result->m_distance = m_distance;
  result->m_zoom = m_zoom;
  result->m_apertureRadius = m_apertureRadius;
  result->m_focalDistance = m_focalDistance;
  return result;
}

const char* ThinLensCamera::fingerprintType() const {
  return "ThinLensCamera";
}

Vector3d ThinLensCamera::eyeOrigin() const {
  const Matrix4d& cameraMatrix = matrix();
  return Vector3d(cameraMatrix.cell(0, 3) - cameraMatrix.cell(0, 2) * m_distance,
                  cameraMatrix.cell(1, 3) - cameraMatrix.cell(1, 2) * m_distance,
                  cameraMatrix.cell(2, 3) - cameraMatrix.cell(2, 2) * m_distance);
}

Rayd ThinLensCamera::rayForPixel(double x, double y, ::render::SampleStream& stream) const {
  // Pull the lens-disc sample from the stream's next 2D dimension.
  // During normal rendering, the renderer has already consumed pixel
  // jitter and shutter time, so this cursor read reaches the same slot
  // named by SampleDimension::Lens while preserving direct-call stream
  // behavior. Map [0, 1]² → [-1, 1]² and route through the concentric
  // square-to-disc mapping (Shirley 1997) above; the disc samples
  // inherit whatever stratification the active sampler provides
  // (jittered, multi-jittered, future Sobol, …).
  //
  // Why stratification matters: pure-random per-call lens sampling
  // gives O(1/√N) Monte Carlo noise on bokeh — at 1024 spp that's
  // ~3% std dev, clearly visible as graininess in out-of-focus
  // regions. With stratified lens samples the dominant
  // pixel-coverage noise drops to O(1/N) (see Pharr & Humphreys,
  // "Physically Based Rendering" §6.2.3).
  const Vector2d lens = stream.next2D();
  const Vector2d disc = concentricMapToDisc(lens);
  return rayForPixelWithLens(x, y, disc.x(), disc.y());
}

Rayd ThinLensCamera::rayForPixelWithLens(double x, double y, double lensU, double lensV) const {
  // Pinhole reference ray — origin at lens centre, target through the
  // pixel on the viewplane at +z=distance from the eye.
  const Matrix4d& cameraMatrix = matrix();
  Vector3d eyeOrigin = this->eyeOrigin();
  Vector3d pixel = viewPlane()->pixelAt(x, y);
  Vector3d pinholeDir = (pixel - eyeOrigin).normalized();

  // Find where this pinhole ray would hit the focal plane. focalDistance
  // is the distance from the **camera position** (the user-facing
  // location set by setPosition) to the in-focus plane along the
  // forward axis — not the distance from the internal eye/pinhole.
  // This matches what every photography app and engine means by "focal
  // distance": subject 8 m from the camera → focalDistance=8 puts it
  // in focus, regardless of how far behind the image plane the
  // math-side pinhole lives.
  //
  // The eye sits m_distance units behind the camera position along
  // -forward, so the eye-to-focal-plane distance is (m_distance +
  // m_focalDistance). Project the pinhole ray onto the forward axis to
  // get t — that's how far along the ray we have to travel to reach
  // the focal plane.
  Vector3d forward = cameraMatrix.transformDirection(Vector3d(0, 0, 1));
  double t = (m_distance + m_focalDistance) / (pinholeDir * forward);
  Vector3d focalPoint = eyeOrigin + pinholeDir * t;

  // Shift the ray origin along the lens disc; the ray still has to pass
  // through focalPoint so that focal-distance geometry stays sharp. The
  // displaced origin lives on a disc of radius apertureRadius oriented
  // along the camera's local x/y plane.
  Vector3d right = cameraMatrix.transformDirection(Vector3d(1, 0, 0));
  Vector3d up = cameraMatrix.transformDirection(Vector3d(0, 1, 0));
  Vector3d lensOffset = (right * lensU + up * lensV) * m_apertureRadius;
  Vector3d lensOrigin = eyeOrigin + lensOffset;

  return Rayd(lensOrigin, (focalPoint - lensOrigin).normalized());
}

std::unique_ptr<Camera::PrimaryRayGenerator> ThinLensCamera::primaryRayGenerator() const {
  class ThinLensPrimaryRayGenerator final : public Camera::PrimaryRayGenerator {
  public:
    ThinLensPrimaryRayGenerator(std::shared_ptr<render::ViewPlane> plane, const Vector3d& eyeOrigin,
                                const Vector3d& forward, const Vector3d& right, const Vector3d& up,
                                double distance, double focalDistance, double apertureRadius)
        : m_plane(std::move(plane)),
          m_eyeOrigin(eyeOrigin),
          m_forward(forward),
          m_right(right),
          m_up(up),
          m_distance(distance),
          m_focalDistance(focalDistance),
          m_apertureRadius(apertureRadius) {
    }

    std::optional<Camera::PrimaryRay> sample(const render::ViewPlane::Iterator& pixel,
                                             render::SampleStream& stream) const override {
      const render::SampleStream::PrimarySample primarySample = stream.primarySample();
      const Vector2d xy = pixel.pixel() + primarySample.pixel;

      const Vector2d lens = stream.next2D();
      const Vector2d disc = concentricMapToDisc(lens);

      const Vector3d pixelPoint = m_plane->pixelAt(xy.x(), xy.y());
      const Vector3d pinholeDirection = (pixelPoint - m_eyeOrigin).normalized();
      const double t = (m_distance + m_focalDistance) / (pinholeDirection * m_forward);
      const Vector3d focalPoint = m_eyeOrigin + pinholeDirection * t;
      const Vector3d lensOffset = (m_right * disc.x() + m_up * disc.y()) * m_apertureRadius;
      const Vector3d lensOrigin = m_eyeOrigin + lensOffset;
      const Rayd ray(lensOrigin, (focalPoint - lensOrigin).normalized());
      if (!ray.direction().isDefined()) {
        return std::nullopt;
      }

      return Camera::PrimaryRay{ray, primarySample.time};
    }

  private:
    std::shared_ptr<render::ViewPlane> m_plane;
    Vector3d m_eyeOrigin;
    Vector3d m_forward;
    Vector3d m_right;
    Vector3d m_up;
    double m_distance;
    double m_focalDistance;
    double m_apertureRadius;
  };

  const Matrix4d& cameraMatrix = matrix();
  return std::make_unique<ThinLensPrimaryRayGenerator>(
    viewPlane(), eyeOrigin(), cameraMatrix.transformDirection(Vector3d(0, 0, 1)),
    cameraMatrix.transformDirection(Vector3d(1, 0, 0)),
    cameraMatrix.transformDirection(Vector3d(0, 1, 0)), m_distance, m_focalDistance,
    m_apertureRadius);
}

std::optional<GpuPrimaryPathDescriptor>
ThinLensCamera::gpuPrimaryPathDescriptor(const Recti& rect, std::uint32_t sampleSeed) const {
  auto plane = viewPlane();
  if (!plane || !plane->sampler() || plane->sampler()->numSamples() <= 0) {
    return std::nullopt;
  }
  if (animationTrack("distance") || animationTrack("zoom") || animationTrack("apertureRadius") ||
      animationTrack("focalDistance")) {
    return std::nullopt;
  }
  std::optional<ThinLensDescriptorMotion> motion;
  if (const std::optional<Matrix4d> descriptorMatrix = fixedShutterGpuCameraMatrix()) {
    motion = ThinLensDescriptorMotion{gpuPrimaryPathMotionModeOriginDelta, *descriptorMatrix,
                                      eyeOriginForMatrix(*descriptorMatrix, m_distance)};
  } else {
    if (const std::optional<detail::SampledShutterDescriptorMotion> stableMotion =
          detail::sampledStableBasisShutterMotion(*this);
        stableMotion) {
      motion =
        ThinLensDescriptorMotion{gpuPrimaryPathMotionModeOriginDelta, stableMotion->matrixAtOpen,
                                 eyeOriginForMatrix(stableMotion->matrixAtOpen, m_distance),
                                 eyeOriginForMatrix(stableMotion->matrixAtClose, m_distance) -
                                   eyeOriginForMatrix(stableMotion->matrixAtOpen, m_distance)};
    } else if (const std::optional<detail::SampledShutterLookAtDescriptorMotion> lookAtMotion =
                 detail::sampledLookAtShutterMotion(*this);
               lookAtMotion) {
      motion = ThinLensDescriptorMotion{
        gpuPrimaryPathMotionModeLookAt,
        Matrix4d::lookAt(lookAtMotion->positionAtOpen, lookAtMotion->targetAtOpen, Vector3d::up()),
        lookAtMotion->positionAtOpen,
        lookAtMotion->positionDelta(),
        lookAtMotion->targetAtOpen,
        lookAtMotion->targetDelta()};
    }
  }
  if (!motion) {
    return std::nullopt;
  }

  const Recti actual = renderableRect(rect);
  if (actual.width() <= 0 || actual.height() <= 0) {
    return std::nullopt;
  }

  checkGpuPathCount(actual, plane->sampler()->numSamples());

  const Vector3d forward = motion->matrixAtOpen.transformDirection(Vector3d(0, 0, 1)).normalized();
  const Vector3d right = motion->matrixAtOpen.transformDirection(Vector3d(1, 0, 0));
  const Vector3d up = motion->matrixAtOpen.transformDirection(Vector3d(0, 1, 0));

  auto descriptorPlane = plane->clone();
  descriptorPlane->setup(motion->planeMatrix(), plane->window());

  GpuPrimaryPathDescriptor descriptor;
  descriptor.mode = gpuPrimaryPathGenerationModeThinLens;
  descriptor.rectilinear.motionMode = motion->motionMode;
  descriptor.rectilinear.originOrDirection = vector4(motion->originOrPosition, 1.0f);
  descriptor.rectilinear.motionOriginDelta = vector4(motion->motionOriginOrPositionDelta, 0.0f);
  descriptor.rectilinear.motionTarget = vector4(motion->target, 1.0f);
  descriptor.rectilinear.motionTargetDelta = vector4(motion->targetDelta, 0.0f);
  descriptor.rectilinear.motionParameters = parameters4(m_distance, m_apertureRadius);
  descriptor.rectilinear.topLeft = vector4(descriptorPlane->pixelAt(0.0, 0.0), 1.0f);
  descriptor.rectilinear.right =
    vector4(descriptorPlane->pixelAt(1.0, 0.0) - descriptorPlane->pixelAt(0.0, 0.0), 0.0f);
  descriptor.rectilinear.down =
    vector4(descriptorPlane->pixelAt(0.0, 1.0) - descriptorPlane->pixelAt(0.0, 0.0), 0.0f);
  descriptor.rectilinear.lensRight = vector4(right * m_apertureRadius, 0.0f);
  descriptor.rectilinear.lensUp = vector4(up * m_apertureRadius, 0.0f);
  descriptor.rectilinear.forward = vector4(forward, 0.0f);
  descriptor.rectilinear.lensParameters = parameters4(m_distance + m_focalDistance, 0.0);
  fillGpuDescriptorViewport(descriptor.rectilinear, rect, actual,
                             plane->sampler()->numSamples(), sampleSeed);
  return descriptor;
}

Vector2d ThinLensCamera::projectPoint(const Vector3d& worldPoint) const {
  return detail::PinholeProjection(*this, m_distance).projectPoint(worldPoint);
}

Vector3d ThinLensCamera::projectPointWithDepth(const Vector3d& worldPoint) const {
  return detail::PinholeProjection(*this, m_distance).projectPointWithDepth(worldPoint);
}

Vector4d ThinLensCamera::projectPointToClipSpace(const Vector3d& worldPoint) const {
  return detail::PinholeProjection(*this, m_distance).projectPointToClipSpace(worldPoint);
}

double ThinLensCamera::eyeRelativeDepth(const Vector3d& worldPoint) const {
  return detail::PinholeProjection(*this, m_distance).eyeRelativeDepth(worldPoint);
}

void ThinLensCamera::setViewPlane(std::shared_ptr<render::ViewPlane> plane) {
  Camera::setViewPlane(plane);
  viewPlane()->setPixelSize(1.0 / m_zoom);

  // Auto-install a multi-sample sampler ONLY if the incoming viewplane
  // has the factory-default 1-spp sampler. ThinLens is fundamentally a
  // multi-sample camera — each per-pixel sample picks a fresh point on
  // the lens disc, and the lens-disc samples have to AVERAGE together
  // to produce DOF blur. With 1 sample per pixel, every pixel gets one
  // lens position and the output is pure noise, not bokeh (see the
  // interactive-preview confetti regression that motivated this hook).
  //
  // The numSamples > 1 guard exists because Modeler's
  // RenderWindow attaches the user's chosen sampler to the viewplane
  // BEFORE calling setViewPlane on the camera — without this guard,
  // we'd silently clobber the user's "1024 spp" UI setting back to 16,
  // and the GUI render would look noisier than the rendercli render of
  // the same scene with the same UI-displayed settings.
  if (viewPlane()->sampler()->numSamples() <= 1) {
    auto jittered = std::make_shared<render::JitteredSampler>();
    jittered->setup(16, 83); // 16 spp; 83 sets — same set count rendercli uses
    viewPlane()->setSampler(jittered);
  }
}

static bool dummy = CameraFactory::self().registerClass<ThinLensCamera>("ThinLensCamera");
