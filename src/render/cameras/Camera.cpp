#include "render/cameras/Camera.h"
#include "core/math/Rect.h"
#include "render/viewplanes/ViewPlane.h"
#include "render/viewplanes/PointInterlacedViewPlane.h"
#include "render/samplers/Sampler.h"
#include "render/tonemap/Tonemap.h"
#include "core/Buffer.h"
#include "render/RayCaster.h"
#include "render/State.h"

using namespace render;

Camera::Camera()
  : m_cancelled(false),
    m_showProgressIndicators(false),
    m_viewPlane(std::make_shared<render::PointInterlacedViewPlane>())
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

void Camera::setViewPlane(std::shared_ptr<render::ViewPlane> plane) {
  m_viewPlane = plane;
}

Vector2d Camera::projectPoint(const Vector3d&) const {
  return Vector2d::undefined();
}

Vector3d Camera::projectPointWithDepth(const Vector3d& worldPoint) const {
  // Default implementation: forward to projectPoint and report zero
  // depth. Subclasses with perspective foreshortening (PinholeCamera
  // and inheritors) override to populate the eye-relative distance.
  // Cameras without a closed-form inverse return undefined, which
  // propagates through here.
  Vector2d screen = projectPoint(worldPoint);
  if (screen.isUndefined()) return Vector3d::undefined();
  return Vector3d(screen.x(), screen.y(), 0.0);
}

Vector4d Camera::projectPointToClipSpace(const Vector3d&) const {
  return Vector4d::undefined();
}

double Camera::eyeRelativeDepth(const Vector3d&) const {
  // Default implementation: cameras without a closed-form projection
  // (FishEye, Spherical, …) report zero. The rasterizer's clipper
  // skips triangles whose vertices have undefined projection anyway,
  // so this default is purely a "don't crash" fallback.
  return 0.0;
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

const Matrix4d& Camera::inverseMatrix() const {
  if (!m_inverseMatrix) {
    m_inverseMatrix = matrix().inverted();
  }
  return m_inverseMatrix;
}

void Camera::render(std::shared_ptr<render::RayCaster> raycaster, Buffer<Colord>& buffer) const {
  render(raycaster, buffer, Recti(0, 0, buffer.width(), buffer.height()));
}

void Camera::render(std::shared_ptr<render::RayCaster> raycaster, Buffer<Colord>& buffer, const Recti& rect) const {
  if (isCancelled())
    return;

  auto plane = viewPlane();
  auto sampler = plane->sampler();
  const int samplesPerPixel = sampler->numSamples();
  const double sampleScale = 1.0 / samplesPerPixel;

  for (render::ViewPlane::Iterator pixel = plane->begin(rect), end = plane->end(rect); pixel != end; ++pixel) {
    if (m_showProgressIndicators) {
      // In-progress indicator: pure red HDR pixel. The downstream
      // tonemap maps `Colord(1, 0, 0)` to 0xff0000 for any operator
      // that's identity-on-pure-channels (Linear, Reinhard, ACES
      // all qualify on a single saturated channel).
      plot(buffer, rect, pixel, Colord(1, 0, 0));
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
        render::State state;
        state.timeSample = timeSample;
        pixelColor += raycaster->rayColor(ray, state);
      }
    }

    // Average the accumulated radiance and write the HDR result.
    // No clamping or 8-bit packing here — that's the tonemap pass's
    // job in `Raytracer::render(Buffer<unsigned int>&)`. Direct
    // float-buffer consumers (EXR writers, future path-tracing
    // accumulators) get the unclipped value.
    plot(buffer, rect, pixel, pixelColor * sampleScale);

    if (isCancelled())
      break;
  }
}

void Camera::plot(Buffer<Colord>& buffer, const Recti& rect, const render::ViewPlane::Iterator& pixel, const Colord& color) const {
  int size = pixel.pixelSize();
  if (size == 1) {
    buffer[pixel.row()][pixel.column()] = color;
  } else {
    for (int x = pixel.column(); x != pixel.column() + size && x < rect.right(); ++x)
      for (int y = pixel.row(); y != pixel.row() + size && y < rect.bottom(); ++y)
        buffer[y][x] = color;
  }
}

void Camera::plotRGB(Buffer<unsigned int>& buffer, const Recti& rect, const render::ViewPlane::Iterator& pixel, unsigned int rgb) const {
  int size = pixel.pixelSize();
  if (size == 1) {
    buffer[pixel.row()][pixel.column()] = rgb;
  } else {
    for (int x = pixel.column(); x != pixel.column() + size && x < rect.right(); ++x)
      for (int y = pixel.row(); y != pixel.row() + size && y < rect.bottom(); ++y)
        buffer[y][x] = rgb;
  }
}

void Camera::render(std::shared_ptr<render::RayCaster> raycaster, Buffer<unsigned int>& buffer,
                    std::shared_ptr<render::Tonemap> tonemap, const Recti& rect) const {
  if (isCancelled())
    return;

  auto plane = viewPlane();
  auto sampler = plane->sampler();
  const int samplesPerPixel = sampler->numSamples();
  const double sampleScale = 1.0 / samplesPerPixel;

  // Mirrors the HDR-buffer render loop above; the only difference is
  // the per-pixel tonemap + pack to packed RGB so the LDR display
  // buffer carries values that the GUI can render immediately. The
  // duplication here is the price of progressive display — see
  // `Camera::render(Buffer<Colord>&, ...)` for the documented
  // sample-stream contract that the two paths share.
  for (render::ViewPlane::Iterator pixel = plane->begin(rect), end = plane->end(rect); pixel != end; ++pixel) {
    if (m_showProgressIndicators) {
      // Pure red (0xff0000) on a saturated channel — every standard
      // tonemap operator (Linear, Reinhard, ACES) maps this to
      // 0xff0000 unchanged.
      plotRGB(buffer, rect, pixel, 0xffff0000);
    }

    const std::uint64_t pixelHash =
      static_cast<std::uint64_t>(pixel.column()) * 73856093ull
      ^ static_cast<std::uint64_t>(pixel.row()) * 19349663ull;

    Colord pixelColor;
    for (int sampleIndex = 0; sampleIndex != samplesPerPixel; ++sampleIndex) {
      auto stream = sampler->stream(sampleIndex, pixelHash);

      Vector2d subPixel = stream->next2D();
      Vector2d xy = pixel.pixel() + subPixel;
      double timeSample = stream->next1D();

      Rayd ray = rayForPixel(xy.x(), xy.y(), *stream);
      if (ray.direction().isDefined()) {
        render::State state;
        state.timeSample = timeSample;
        pixelColor += raycaster->rayColor(ray, state);
      }
    }

    Colord averaged = pixelColor * sampleScale;
    unsigned int rgb = (tonemap ? tonemap->apply(averaged) : averaged).rgb();
    plotRGB(buffer, rect, pixel, rgb);

    if (isCancelled())
      break;
  }
}
