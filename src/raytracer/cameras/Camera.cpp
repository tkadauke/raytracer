#include "raytracer/cameras/Camera.h"
#include "core/math/Rect.h"
#include "raytracer/viewplanes/ViewPlane.h"
#include "raytracer/viewplanes/PointInterlacedViewPlane.h"
#include "raytracer/samplers/Sampler.h"
#include "core/Buffer.h"
#include "raytracer/Raytracer.h"
#include "raytracer/State.h"

using namespace raytracer;

Camera::Camera()
  : m_cancelled(false),
    m_showProgressIndicators(false),
    m_viewPlane(std::make_shared<PointInterlacedViewPlane>())
{
}

Camera::Camera(const Vector3d& position, const Vector3d& target)
  : Camera()
{
  m_position = position;
  m_target = target;
}

Camera::~Camera() {
}

void Camera::setViewPlane(std::shared_ptr<ViewPlane> plane) {
  m_viewPlane = plane;
}

const Matrix4d& Camera::matrix() const {
  if (!m_matrix) {
    auto zAxis = (m_target - m_position).normalized();
    auto xAxis = Vector3d::up() ^ zAxis;
    auto yAxis = xAxis ^ -zAxis;

    m_matrix = Matrix4d(xAxis, yAxis, zAxis).inverted();
    m_matrix.value().setCell(0, 3, m_position[0]);
    m_matrix.value().setCell(1, 3, m_position[1]);
    m_matrix.value().setCell(2, 3, m_position[2]);
  }
  return m_matrix;
}

void Camera::render(std::shared_ptr<Raytracer> raytracer, Buffer<unsigned int>& buffer) const {
  render(raytracer, buffer, Recti(0, 0, buffer.width(), buffer.height()));
}

void Camera::render(std::shared_ptr<Raytracer> raytracer, Buffer<unsigned int>& buffer, const Recti& rect) const {
  if (isCancelled())
    return;

  auto plane = viewPlane();
  auto sampler = plane->sampler();
  const int samplesPerPixel = sampler->numSamples();

  for (ViewPlane::Iterator pixel = plane->begin(rect), end = plane->end(rect); pixel != end; ++pixel) {
    if (m_showProgressIndicators) {
      plotRGB(buffer, rect, pixel, 0xff0000);
    }

    // Per-pixel hash: any cheap function that varies per (column,
    // row) is fine. Weyl-style multipliers spread adjacent pixels
    // into different sample sets so neighbouring pixels don't end up
    // with identical lens / time / ... dimensions for the same
    // sampleIndex. The constants are coprime odd-ish — the exact
    // values don't matter, only that they decorrelate the grid.
    const std::uint64_t pixelHash =
      static_cast<std::uint64_t>(pixel.column()) * 73856093ull
      ^ static_cast<std::uint64_t>(pixel.row()) * 19349663ull;

    Colord pixelColor;
    for (int sampleIndex = 0; sampleIndex != samplesPerPixel; ++sampleIndex) {
      auto stream = sampler->stream(sampleIndex, pixelHash);

      // Dimensions 0 and 1 of the stream are owned by the renderer
      // and consumed before the camera sees the stream:
      //   dim 0 (2D) — sub-pixel jitter for anti-aliasing.
      //   dim 1 (1D) — shutter-time sample, in [0, 1). Animatable
      //                primitives (Instance with non-zero velocity)
      //                read this from `state.timeSample` and
      //                interpolate their transforms.
      // Cameras therefore see a stream starting at dimension 2;
      // whatever they pull (lens disc, future Kolb element index,
      // ...) is decorrelated from sub-pixel and time.
      Vector2d subPixel = stream->next2D();
      Vector2d xy = pixel.pixel() + subPixel;
      double timeSample = stream->next1D();

      Rayd ray = rayForPixel(xy.x(), xy.y(), *stream);
      if (ray.direction().isDefined()) {
        State state;
        state.timeSample = timeSample;
        pixelColor += raytracer->rayColor(ray, state);
      }
    }

    plot(buffer, rect, pixel, pixelColor);

    if (isCancelled())
      break;
  }
}

void Camera::plot(Buffer<unsigned int>& buffer, const Recti& rect, const ViewPlane::Iterator& pixel, const Colord& color) const {
  auto avergageColor = color / viewPlane()->sampler()->numSamples();
  unsigned int rgb = avergageColor.rgb();
  plotRGB(buffer, rect, pixel, rgb);
}

void Camera::plotRGB(Buffer<unsigned int>& buffer, const Recti& rect, const ViewPlane::Iterator& pixel, unsigned int rgbColor) const {
  int size = pixel.pixelSize();
  if (size == 1) {
    buffer[pixel.row()][pixel.column()] = rgbColor;
  } else {
    for (int x = pixel.column(); x != pixel.column() + size && x < rect.right(); ++x)
      for (int y = pixel.row(); y != pixel.row() + size && y < rect.bottom(); ++y)
        buffer[y][x] = rgbColor;
  }
}
