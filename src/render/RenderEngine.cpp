#include "render/RenderEngine.h"
using namespace render;
#include "render/cameras/PinholeCamera.h"
#include "render/primitives/Scene.h"
#include "render/tonemap/LinearTonemap.h"
#include "core/Buffer.h"

struct RenderEngine::Private {
  inline Private()
      : tonemap(std::make_shared<render::LinearTonemap>()) {
  }

  std::shared_ptr<render::Tonemap> tonemap;
  bool progressiveDisplayEnabled{true};
  double animationFrame{0.0};
  double shutterOpen{0.0};
  double shutterClose{0.0};
};

RenderEngine::RenderEngine(std::shared_ptr<render::Scene> scene)
    : m_camera(std::make_shared<render::PinholeCamera>()),
      m_scene(std::move(scene)),
      p(std::make_unique<Private>()) {
}

RenderEngine::RenderEngine(std::shared_ptr<render::Camera> camera,
                           std::shared_ptr<render::Scene> scene)
    : m_camera(std::move(camera)),
      m_scene(std::move(scene)),
      p(std::make_unique<Private>()) {
}

RenderEngine::~RenderEngine() {
}

std::shared_ptr<RenderEngine> RenderEngine::cloneForRender() const {
  return nullptr;
}

void RenderEngine::render(Buffer<unsigned int>& displayBuffer) {
  // Allocate the float HDR accumulator that the engine writes
  // into, run the engine-specific render, then walk every pixel
  // applying the configured tonemap and packing to RGB. The
  // two-buffer cost (~24 bytes/pixel for the HDR accumulator on
  // top of the display buffer) is the price of putting the
  // tonemap stage outside the per-tile worker — see the equivalent
  // comment that used to live in Raytracer for the tradeoff.
  Buffer<Colord> hdr(displayBuffer.width(), displayBuffer.height());
  render(hdr);

  auto tonemap = p->tonemap;
  for (int y = 0; y < hdr.height(); ++y) {
    for (int x = 0; x < hdr.width(); ++x) {
      displayBuffer[y][x] = tonemap->apply(hdr[y][x]).rgb();
    }
  }
}

std::shared_ptr<render::Tonemap> RenderEngine::tonemap() const {
  return p->tonemap;
}

void RenderEngine::setCamera(std::shared_ptr<render::Camera> camera) {
  m_camera = std::move(camera);
  if (m_camera) {
    m_camera->setAnimationFrame(p->animationFrame);
    m_camera->setShutterInterval(p->shutterOpen, p->shutterClose);
  }
}

void RenderEngine::setTonemap(std::shared_ptr<render::Tonemap> tonemap) {
  p->tonemap = std::move(tonemap);
}

void RenderEngine::setProgressiveDisplayEnabled(bool enabled) {
  p->progressiveDisplayEnabled = enabled;
}

bool RenderEngine::progressiveDisplayEnabled() const {
  return p->progressiveDisplayEnabled;
}

void RenderEngine::setAnimationFrame(double frame) {
  p->animationFrame = frame;
  if (m_camera)
    m_camera->setAnimationFrame(frame);
}

double RenderEngine::animationFrame() const {
  return p->animationFrame;
}

void RenderEngine::setShutterInterval(double open, double close) {
  p->shutterOpen = open;
  p->shutterClose = close;
  if (m_camera)
    m_camera->setShutterInterval(open, close);
}

double RenderEngine::shutterOpen() const {
  return p->shutterOpen;
}

double RenderEngine::shutterClose() const {
  return p->shutterClose;
}

std::list<Recti> RenderEngine::activeTiles() const {
  return {};
}

std::list<Recti> RenderEngine::completedTiles() const {
  return {};
}

Colord RenderEngine::backgroundColor() const {
  if (m_backgroundColorOverride)
    return *m_backgroundColorOverride;
  if (m_scene)
    return m_scene->background();
  return Colord::black();
}

void RenderEngine::setBackgroundColor(const Colord& color) {
  m_backgroundColorOverride = color;
}

void RenderEngine::clearBackgroundColor() {
  m_backgroundColorOverride.reset();
}

bool RenderEngine::hasBackgroundColorOverride() const {
  return m_backgroundColorOverride.has_value();
}

void RenderEngine::copyRenderEngineStateTo(RenderEngine& engine) const {
  engine.setTonemap(tonemap());
  engine.setAnimationFrame(animationFrame());
  engine.setShutterInterval(shutterOpen(), shutterClose());
  if (hasBackgroundColorOverride()) {
    engine.setBackgroundColor(backgroundColor());
  }
  engine.setProgressiveDisplayEnabled(progressiveDisplayEnabled());
}
