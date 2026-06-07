#include "render/cameras/CameraFactory.h"
#include "core/DivisionByZeroException.h"
#include "render/cameras/PinholeCamera.h"
#include "PinholeProjection.h"
#include "core/math/Ray.h"
#include "render/viewplanes/ViewPlane.h"

#include <optional>
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
                               const PinholeCamera& camera,
                               const Vector3d& origin)
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
