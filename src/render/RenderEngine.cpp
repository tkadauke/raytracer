#include "render/RenderEngine.h"
using namespace render;
#include "render/cameras/PinholeCamera.h"
#include "render/tonemap/LinearTonemap.h"
#include "core/Buffer.h"


struct RenderEngine::Private {
  inline Private()
    : tonemap(std::make_shared<render::LinearTonemap>())
  {
  }

  std::shared_ptr<render::Tonemap> tonemap;
};

RenderEngine::RenderEngine(std::shared_ptr<render::Scene> scene)
  : m_camera(std::make_shared<render::PinholeCamera>()),
    m_scene(std::move(scene)),
    p(std::make_unique<Private>())
{
}

RenderEngine::RenderEngine(std::shared_ptr<render::Camera> camera, std::shared_ptr<render::Scene> scene)
  : m_camera(std::move(camera)),
    m_scene(std::move(scene)),
    p(std::make_unique<Private>())
{
}

RenderEngine::~RenderEngine() {
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

void RenderEngine::setTonemap(std::shared_ptr<render::Tonemap> tonemap) {
  p->tonemap = std::move(tonemap);
}

std::list<Recti> RenderEngine::activeRects() const {
  return {};
}
