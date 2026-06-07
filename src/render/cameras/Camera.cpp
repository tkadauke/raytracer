#include "render/cameras/Camera.h"
#include "core/math/Rect.h"
#include "render/SamplingSeed.h"
#include "render/viewplanes/ViewPlane.h"
#include "render/viewplanes/PointInterlacedViewPlane.h"
#include "render/samplers/Sampler.h"
#include "render/tonemap/Tonemap.h"
#include "core/Buffer.h"
#include "render/RayCaster.h"
#include "render/State.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

using namespace render;

namespace {
  std::uint64_t legacyPixelHash(const render::ViewPlane::Iterator& pixel) {
    // Per-pixel hash: any cheap function that varies per (column,
    // row) is fine. Weyl-style multipliers spread adjacent pixels
    // into different sample sets so neighbouring pixels don't end up
    // with identical lens / time / ... dimensions for the same
    // sampleIndex. The constants are coprime odd-ish — the exact
    // values don't matter, only that they decorrelate the grid.
    return static_cast<std::uint64_t>(pixel.column()) * 73856093ull ^
           static_cast<std::uint64_t>(pixel.row()) * 19349663ull;
  }

  std::uint64_t pixelHashFor(const render::ViewPlane::Iterator& pixel,
                             std::optional<std::uint64_t> tileSeed) {
    if (tileSeed)
      return render::SamplingSeed::pixelSeed(*tileSeed, pixel.column(), pixel.row());
    return legacyPixelHash(pixel);
  }
}

Camera::Camera()
    : m_cancelled(false),
      m_showProgressIndicators(false),
      m_aspectMode(render::AspectMode::Stretch),
      m_aspectRatio(0.0),
      m_viewPlane(std::make_shared<render::PointInterlacedViewPlane>()) {
}

Camera::Camera(const Vector3d& position, const Vector3d& target)
    : Camera() {
  m_position = position;
  m_target = target;
}

Camera::~Camera() {
}

Camera::PrimaryRayGenerator::~PrimaryRayGenerator() = default;

void Camera::copyBaseStateTo(Camera& camera) const {
  camera.setName(name());
  camera.setMetadata(metadata());
  for (const auto& [propertyName, track] : animationTracks()) {
    camera.setAnimationTrack(propertyName, track);
  }
  camera.m_cancelled.store(false, std::memory_order_release);
  camera.m_showProgressIndicators = m_showProgressIndicators;
  camera.m_aspectMode = m_aspectMode;
  camera.m_aspectRatio = m_aspectRatio;
  camera.m_position = m_position;
  camera.m_target = m_target;
  camera.m_animationFrame = m_animationFrame;
  camera.m_shutterOpen = m_shutterOpen;
  camera.m_shutterClose = m_shutterClose;
  camera.m_matrix.reset();
  camera.m_inverseMatrix.reset();
  // The cloned view plane already carries m_aspectMode / m_aspectRatio
  // because clone() copies all fields; the camera-side copies above
  // keep the two in sync if setViewPlane() is called on the clone later.
  camera.m_viewPlane = m_viewPlane ? m_viewPlane->clone() : nullptr;
}

void Camera::setViewPlane(std::shared_ptr<render::ViewPlane> plane) {
  m_viewPlane = plane;
  if (m_viewPlane) {
    m_viewPlane->setAspectMode(m_aspectMode);
    m_viewPlane->setAspectRatio(m_aspectRatio);
  }
}

void Camera::setAspectMode(render::AspectMode mode) {
  m_aspectMode = mode;
  if (m_viewPlane)
    m_viewPlane->setAspectMode(mode);
}

render::AspectMode Camera::aspectMode() const {
  return m_aspectMode;
}

Recti Camera::renderableRect(const Recti& rect) const {
  auto plane = viewPlane();
  if (!plane || plane->aspectMode() != render::AspectMode::FitExact) {
    return rect;
  }

  const Recti& inner = plane->innerRect();
  const int left = std::max(rect.left(), inner.left());
  const int top = std::max(rect.top(), inner.top());
  const int right = std::min(rect.right(), inner.right());
  const int bottom = std::min(rect.bottom(), inner.bottom());
  if (left >= right || top >= bottom) {
    return Recti(left, top, 0, 0);
  }
  return Recti(left, top, right - left, bottom - top);
}

int Camera::samplesPerPixel() const {
  return viewPlane()->sampler()->numSamples();
}

std::optional<Camera::PrimaryRaySample>
Camera::primaryRaySample(const render::ViewPlane::Iterator& pixel, int sampleIndex,
                         std::optional<std::uint64_t> tileSeed) const {
  auto stream =
    viewPlane()->sampler()->sharedStream(sampleIndex, primaryRayPixelHash(pixel, tileSeed));

  if (auto sample = primaryRaySample(pixel, *stream)) {
    return PrimaryRaySample{sample->ray, sample->timeSample, sample->animationTime,
                            std::move(stream)};
  }
  return std::nullopt;
}

std::optional<Camera::PrimaryRay> Camera::primaryRaySample(const render::ViewPlane::Iterator& pixel,
                                                           render::SampleStream& stream) const {
  // The renderer owns the pixel and time dimensions and consumes
  // them before the camera sees the stream:
  //   Pixel (2D) — sub-pixel jitter for anti-aliasing.
  //   Time  (1D) — shutter-time sample, in [0, 1). Animatable
  //                primitives read this from `state.timeSample`
  //                and interpolate their transforms.
  // The sequential cursor is therefore positioned at the historical
  // lens/camera dimension; explicit `SampleDimension` accessors use
  // the same stable ownership without depending on call order.
  const render::SampleStream::PrimarySample primarySample = stream.primarySample();
  Vector2d xy = pixel.pixel() + primarySample.pixel;

  Rayd ray = rayForPixel(xy.x(), xy.y(), stream);
  if (!ray.direction().isDefined()) {
    return std::nullopt;
  }

  return PrimaryRay{ray, primarySample.time, animationTimeForSample(primarySample.time)};
}

std::unique_ptr<Camera::PrimaryRayGenerator> Camera::primaryRayGenerator() const {
  class DefaultPrimaryRayGenerator final : public Camera::PrimaryRayGenerator {
  public:
    explicit DefaultPrimaryRayGenerator(const Camera& camera)
        : m_camera(camera) {
    }

    std::optional<Camera::PrimaryRay> sample(const render::ViewPlane::Iterator& pixel,
                                             render::SampleStream& stream) const override {
      return m_camera.primaryRaySample(pixel, stream);
    }

  private:
    const Camera& m_camera;
  };

  return std::make_unique<DefaultPrimaryRayGenerator>(*this);
}

std::uint64_t Camera::primaryRayPixelHash(const render::ViewPlane::Iterator& pixel,
                                          std::optional<std::uint64_t> tileSeed) const {
  return pixelHashFor(pixel, tileSeed);
}

void Camera::setAspectRatio(double ratio) {
  m_aspectRatio = ratio;
  if (m_viewPlane)
    m_viewPlane->setAspectRatio(ratio);
}

double Camera::aspectRatio() const {
  return m_aspectRatio;
}

void Camera::setAnimationFrame(double frame) {
  m_animationFrame = frame;
}

double Camera::animationFrame() const {
  return m_animationFrame;
}

void Camera::setShutterInterval(double open, double close) {
  m_shutterOpen = open;
  m_shutterClose = close;
}

double Camera::shutterOpen() const {
  return m_shutterOpen;
}

double Camera::shutterClose() const {
  return m_shutterClose;
}

double Camera::animationTimeForSample(double timeSample) const {
  return m_animationFrame + m_shutterOpen + (m_shutterClose - m_shutterOpen) * timeSample;
}

Vector2d Camera::projectPoint(const Vector3d&) const {
  return Vector2d::undefined;
}

Vector3d Camera::projectPointWithDepth(const Vector3d& worldPoint) const {
  // Default implementation: forward to projectPoint and report zero
  // depth. Subclasses with perspective foreshortening (PinholeCamera
  // and inheritors) override to populate the eye-relative distance.
  // Cameras without a closed-form inverse return undefined, which
  // propagates through here.
  Vector2d screen = projectPoint(worldPoint);
  if (screen.isUndefined())
    return Vector3d::undefined;
  return Vector3d(screen.x(), screen.y(), 0.0);
}

Vector4d Camera::projectPointToClipSpace(const Vector3d&) const {
  return Vector4d::undefined;
}

std::optional<Matrix4d> Camera::worldToClipMatrix() const {
  return std::nullopt;
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
    m_matrix = Matrix4d::lookAt(m_position, m_target, Vector3d::up());
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

void Camera::render(std::shared_ptr<render::RayCaster> raycaster, Buffer<Colord>& buffer,
                    const Recti& rect) const {
  render(raycaster, buffer, rect, std::nullopt);
}

void Camera::render(std::shared_ptr<render::RayCaster> raycaster, Buffer<Colord>& buffer,
                    const Recti& rect, std::uint64_t tileSeed) const {
  render(raycaster, buffer, rect, std::optional<std::uint64_t>(tileSeed));
}

void Camera::render(std::shared_ptr<render::RayCaster> raycaster, Buffer<Colord>& buffer,
                    const Recti& rect, std::optional<std::uint64_t> tileSeed) const {
  if (isCancelled())
    return;

  auto plane = viewPlane();
  Recti actualRect = renderableRect(rect);
  if (actualRect.width() <= 0 || actualRect.height() <= 0)
    return;

  const int sampleCount = samplesPerPixel();
  const double sampleScale = 1.0 / sampleCount;

  if (sampleCount > 1 && raycaster->prefersProgressiveSamplePublishing()) {
    renderProgressiveSamples(raycaster, buffer, actualRect, tileSeed);
    return;
  }

  for (render::ViewPlane::Iterator pixel = plane->begin(actualRect), end = plane->end(actualRect);
       pixel != end; ++pixel) {
    if (isCancelled())
      break;

    if (m_showProgressIndicators) {
      // In-progress indicator: pure red HDR pixel. The downstream
      // tonemap maps `Colord(1, 0, 0)` to 0xff0000 for any operator
      // that's identity-on-pure-channels (Linear, Reinhard, ACES
      // all qualify on a single saturated channel).
      plot(buffer, actualRect, pixel, Colord(1, 0, 0));
    }

    Colord pixelColor;
    for (int sampleIndex = 0; sampleIndex != sampleCount; ++sampleIndex) {
      if (isCancelled())
        break;

      if (auto sample = primaryRaySample(pixel, sampleIndex, tileSeed)) {
        render::State state;
        state.timeSample = sample->timeSample;
        state.animationFrame = animationFrame();
        state.animationTime = sample->animationTime;
        state.sampleStream = sample->sampleStream.get();
        pixelColor += raycaster->rayColor(sample->ray, state);
      }
    }

    if (isCancelled())
      break;

    // Average the accumulated radiance and write the HDR result.
    // No clamping or 8-bit packing here — that's the tonemap pass's
    // job in `Raytracer::render(Buffer<unsigned int>&)`. Direct
    // float-buffer consumers (EXR writers, future path-tracing
    // accumulators) get the unclipped value.
    plot(buffer, actualRect, pixel, pixelColor * sampleScale);

    if (isCancelled())
      break;
  }
}

void Camera::plot(Buffer<Colord>& buffer, const Recti& rect,
                  const render::ViewPlane::Iterator& pixel, const Colord& color) const {
  const Recti footprint = pixel.footprintWithin(rect);
  for (int y = footprint.top(); y != footprint.bottom(); ++y)
    for (int x = footprint.left(); x != footprint.right(); ++x)
      buffer[y][x] = color;
}

void Camera::plotRGB(Buffer<unsigned int>& buffer, const Recti& rect,
                     const render::ViewPlane::Iterator& pixel, unsigned int rgb) const {
  const Recti footprint = pixel.footprintWithin(rect);
  for (int y = footprint.top(); y != footprint.bottom(); ++y)
    for (int x = footprint.left(); x != footprint.right(); ++x)
      buffer[y][x] = rgb;
}

std::optional<Colord> Camera::sampleRayColor(std::shared_ptr<render::RayCaster> raycaster,
                                             const render::ViewPlane::Iterator& pixel,
                                             int sampleIndex,
                                             std::optional<std::uint64_t> tileSeed) const {
  const auto sample = primaryRaySample(pixel, sampleIndex, tileSeed);
  if (!sample) {
    return std::nullopt;
  }

  render::State state;
  state.timeSample = sample->timeSample;
  state.sampleStream = sample->sampleStream.get();
  return raycaster->rayColor(sample->ray, state);
}

std::size_t Camera::accumulationIndex(const Recti& rect,
                                      const render::ViewPlane::Iterator& pixel) const {
  return static_cast<std::size_t>(pixel.row() - rect.top()) *
           static_cast<std::size_t>(rect.width()) +
         static_cast<std::size_t>(pixel.column() - rect.left());
}

void Camera::renderProgressiveSamples(std::shared_ptr<render::RayCaster> raycaster,
                                      Buffer<Colord>& buffer, const Recti& rect,
                                      std::optional<std::uint64_t> tileSeed) const {
  std::vector<Colord> accumulated(static_cast<std::size_t>(rect.width() * rect.height()),
                                  Colord::black());
  const int sampleCount = samplesPerPixel();

  for (int sampleIndex = 0; sampleIndex != sampleCount; ++sampleIndex) {
    for (render::ViewPlane::Iterator pixel = viewPlane()->pixelBegin(rect),
                                     end = viewPlane()->end(rect);
         pixel != end; ++pixel) {
      if (isCancelled())
        return;

      const std::size_t index = accumulationIndex(rect, pixel);
      if (const auto color = sampleRayColor(raycaster, pixel, sampleIndex, tileSeed)) {
        accumulated[index] += *color;
      }
    }

    const double sampleScale = 1.0 / (sampleIndex + 1);
    for (render::ViewPlane::Iterator pixel = viewPlane()->pixelBegin(rect),
                                     end = viewPlane()->end(rect);
         pixel != end; ++pixel) {
      if (isCancelled())
        return;

      const std::size_t index = accumulationIndex(rect, pixel);
      plot(buffer, rect, pixel, accumulated[index] * sampleScale);
    }
  }
}

void Camera::renderProgressiveSamples(std::shared_ptr<render::RayCaster> raycaster,
                                      Buffer<unsigned int>& buffer,
                                      std::shared_ptr<render::Tonemap> tonemap, const Recti& rect,
                                      std::optional<std::uint64_t> tileSeed) const {
  std::vector<Colord> accumulated(static_cast<std::size_t>(rect.width() * rect.height()),
                                  Colord::black());
  const int sampleCount = samplesPerPixel();

  for (int sampleIndex = 0; sampleIndex != sampleCount; ++sampleIndex) {
    for (render::ViewPlane::Iterator pixel = viewPlane()->pixelBegin(rect),
                                     end = viewPlane()->end(rect);
         pixel != end; ++pixel) {
      if (isCancelled())
        return;

      const std::size_t index = accumulationIndex(rect, pixel);
      if (const auto color = sampleRayColor(raycaster, pixel, sampleIndex, tileSeed)) {
        accumulated[index] += *color;
      }
    }

    const double sampleScale = 1.0 / (sampleIndex + 1);
    for (render::ViewPlane::Iterator pixel = viewPlane()->pixelBegin(rect),
                                     end = viewPlane()->end(rect);
         pixel != end; ++pixel) {
      if (isCancelled())
        return;

      const std::size_t index = accumulationIndex(rect, pixel);
      const Colord averaged = accumulated[index] * sampleScale;
      const unsigned int rgb = (tonemap ? tonemap->apply(averaged) : averaged).rgb();
      plotRGB(buffer, rect, pixel, rgb);
    }
  }
}

void Camera::renderProgressiveSamples(std::shared_ptr<render::RayCaster> raycaster,
                                      Buffer<Colord>& hdrBuffer,
                                      Buffer<unsigned int>& displayBuffer,
                                      std::shared_ptr<render::Tonemap> tonemap, const Recti& rect,
                                      std::optional<std::uint64_t> tileSeed) const {
  std::vector<Colord> accumulated(static_cast<std::size_t>(rect.width() * rect.height()),
                                  Colord::black());
  const int sampleCount = samplesPerPixel();

  for (int sampleIndex = 0; sampleIndex != sampleCount; ++sampleIndex) {
    for (render::ViewPlane::Iterator pixel = viewPlane()->pixelBegin(rect),
                                     end = viewPlane()->end(rect);
         pixel != end; ++pixel) {
      if (isCancelled())
        return;

      const std::size_t index = accumulationIndex(rect, pixel);
      if (const auto color = sampleRayColor(raycaster, pixel, sampleIndex, tileSeed)) {
        accumulated[index] += *color;
      }
    }

    const double sampleScale = 1.0 / (sampleIndex + 1);
    for (render::ViewPlane::Iterator pixel = viewPlane()->pixelBegin(rect),
                                     end = viewPlane()->end(rect);
         pixel != end; ++pixel) {
      if (isCancelled())
        return;

      const std::size_t index = accumulationIndex(rect, pixel);
      const Colord averaged = accumulated[index] * sampleScale;
      plot(hdrBuffer, rect, pixel, averaged);
      const unsigned int rgb = (tonemap ? tonemap->apply(averaged) : averaged).rgb();
      plotRGB(displayBuffer, rect, pixel, rgb);
    }
  }
}

void Camera::render(std::shared_ptr<render::RayCaster> raycaster, Buffer<unsigned int>& buffer,
                    std::shared_ptr<render::Tonemap> tonemap, const Recti& rect) const {
  render(raycaster, buffer, tonemap, rect, std::nullopt);
}

void Camera::render(std::shared_ptr<render::RayCaster> raycaster, Buffer<unsigned int>& buffer,
                    std::shared_ptr<render::Tonemap> tonemap, const Recti& rect,
                    std::uint64_t tileSeed) const {
  render(raycaster, buffer, tonemap, rect, std::optional<std::uint64_t>(tileSeed));
}

void Camera::render(std::shared_ptr<render::RayCaster> raycaster, Buffer<unsigned int>& buffer,
                    std::shared_ptr<render::Tonemap> tonemap, const Recti& rect,
                    std::optional<std::uint64_t> tileSeed) const {
  if (isCancelled())
    return;

  auto plane = viewPlane();
  Recti actualRect = renderableRect(rect);
  if (actualRect.width() <= 0 || actualRect.height() <= 0)
    return;

  const int sampleCount = samplesPerPixel();
  const double sampleScale = 1.0 / sampleCount;

  if (sampleCount > 1 && raycaster->prefersProgressiveSamplePublishing()) {
    renderProgressiveSamples(raycaster, buffer, tonemap, actualRect, tileSeed);
    return;
  }

  // Mirrors the HDR-buffer render loop above; the only difference is
  // the per-pixel tonemap + pack to packed RGB so the LDR display
  // buffer carries values that the GUI can render immediately. The
  // duplication here is the price of progressive display — see
  // `Camera::render(Buffer<Colord>&, ...)` for the documented
  // sample-stream contract that the two paths share.
  for (render::ViewPlane::Iterator pixel = plane->begin(actualRect), end = plane->end(actualRect);
       pixel != end; ++pixel) {
    if (isCancelled())
      break;

    if (m_showProgressIndicators) {
      // Pure red (0xff0000) on a saturated channel — every standard
      // tonemap operator (Linear, Reinhard, ACES) maps this to
      // 0xff0000 unchanged.
      plotRGB(buffer, actualRect, pixel, 0xffff0000);
    }

    Colord pixelColor;
    for (int sampleIndex = 0; sampleIndex != sampleCount; ++sampleIndex) {
      if (isCancelled())
        break;

      if (auto sample = primaryRaySample(pixel, sampleIndex, tileSeed)) {
        render::State state;
        state.timeSample = sample->timeSample;
        state.animationFrame = animationFrame();
        state.animationTime = sample->animationTime;
        state.sampleStream = sample->sampleStream.get();
        pixelColor += raycaster->rayColor(sample->ray, state);
      }
    }

    if (isCancelled())
      break;

    Colord averaged = pixelColor * sampleScale;
    unsigned int rgb = (tonemap ? tonemap->apply(averaged) : averaged).rgb();
    plotRGB(buffer, actualRect, pixel, rgb);

    if (isCancelled())
      break;
  }
}

void Camera::render(std::shared_ptr<render::RayCaster> raycaster, Buffer<Colord>& hdrBuffer,
                    Buffer<unsigned int>& displayBuffer, std::shared_ptr<render::Tonemap> tonemap,
                    const Recti& rect) const {
  render(raycaster, hdrBuffer, displayBuffer, tonemap, rect, std::nullopt);
}

void Camera::render(std::shared_ptr<render::RayCaster> raycaster, Buffer<Colord>& hdrBuffer,
                    Buffer<unsigned int>& displayBuffer, std::shared_ptr<render::Tonemap> tonemap,
                    const Recti& rect, std::uint64_t tileSeed) const {
  render(raycaster, hdrBuffer, displayBuffer, tonemap, rect,
         std::optional<std::uint64_t>(tileSeed));
}

void Camera::render(std::shared_ptr<render::RayCaster> raycaster, Buffer<Colord>& hdrBuffer,
                    Buffer<unsigned int>& displayBuffer, std::shared_ptr<render::Tonemap> tonemap,
                    const Recti& rect, std::optional<std::uint64_t> tileSeed) const {
  if (isCancelled())
    return;

  auto plane = viewPlane();
  Recti actualRect = renderableRect(rect);
  if (actualRect.width() <= 0 || actualRect.height() <= 0)
    return;

  const int sampleCount = samplesPerPixel();
  const double sampleScale = 1.0 / sampleCount;

  if (sampleCount > 1 && raycaster->prefersProgressiveSamplePublishing()) {
    renderProgressiveSamples(raycaster, hdrBuffer, displayBuffer, tonemap, actualRect, tileSeed);
    return;
  }

  for (render::ViewPlane::Iterator pixel = plane->begin(actualRect), end = plane->end(actualRect);
       pixel != end; ++pixel) {
    if (isCancelled())
      break;

    if (m_showProgressIndicators) {
      plot(hdrBuffer, actualRect, pixel, Colord(1, 0, 0));
      plotRGB(displayBuffer, actualRect, pixel, 0xffff0000);
    }

    Colord pixelColor;
    for (int sampleIndex = 0; sampleIndex != sampleCount; ++sampleIndex) {
      if (isCancelled())
        break;

      if (auto sample = primaryRaySample(pixel, sampleIndex, tileSeed)) {
        render::State state;
        state.timeSample = sample->timeSample;
        state.animationFrame = animationFrame();
        state.animationTime = sample->animationTime;
        state.sampleStream = sample->sampleStream.get();
        pixelColor += raycaster->rayColor(sample->ray, state);
      }
    }

    if (isCancelled())
      break;

    Colord averaged = pixelColor * sampleScale;
    plot(hdrBuffer, actualRect, pixel, averaged);
    unsigned int rgb = (tonemap ? tonemap->apply(averaged) : averaged).rgb();
    plotRGB(displayBuffer, actualRect, pixel, rgb);

    if (isCancelled())
      break;
  }
}
