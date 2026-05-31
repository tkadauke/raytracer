#include "engine/wavefront/WavefrontRaytracer.h"

#include "core/Buffer.h"
#include "core/util/BufferUtils.h"
#include "engine/TileRenderTask.h"
#include "render/Integrator.h"
#include "render/RayCaster.h"
#include "render/SamplingSeed.h"
#include "render/State.h"
#include "render/Stats.h"
#include "render/TilePlan.h"
#include "render/WhittedIntegrator.h"
#include "render/cameras/Camera.h"
#include "render/primitives/Scene.h"
#include "render/tonemap/Tonemap.h"

#include <QThread>
#include <QThreadPool>

#include <iostream>
#include <stdexcept>
#include <utility>

namespace engine::wavefront {
  namespace {
    class RecursiveRayCasterAdapter : public render::RayCaster {
    public:
      RecursiveRayCasterAdapter(const render::Scene& scene, const render::Integrator& integrator)
          : m_scene(scene),
            m_integrator(integrator) {
      }

      Colord rayColor(const Rayd& ray, render::State& state) const override {
        return m_integrator.radiance(m_scene, ray, state, *this);
      }

    private:
      const render::Scene& m_scene;
      const render::Integrator& m_integrator;
    };
  }

  struct WavefrontRaytracer::Private {
    Private()
        : threadPool(std::make_unique<QThreadPool>()),
          queueSize(QThread::idealThreadCount()),
          integrator(std::make_unique<render::WhittedIntegrator>()),
          showProgressIndicators(false) {
    }

    std::unique_ptr<QThreadPool> threadPool;
    std::list<std::shared_ptr<engine::TileRenderTask>> tasks;
    int queueSize;
    std::unique_ptr<render::Integrator> integrator;
    bool showProgressIndicators;
    std::optional<std::uint64_t> samplingSeed;

    Colord tracePixel(const render::Camera& camera, const render::RayCaster& rayCaster,
                      const render::ViewPlane::Iterator& pixel,
                      std::optional<std::uint64_t> tileSeed) const {
      const int sampleCount = camera.samplesPerPixel();
      const double sampleScale = 1.0 / sampleCount;

      Colord pixelColor;
      for (int sampleIndex = 0; sampleIndex != sampleCount; ++sampleIndex) {
        if (camera.isCancelled())
          break;

        if (auto sample = camera.primaryRaySample(pixel, sampleIndex, tileSeed)) {
          render::State state;
          state.timeSample = sample->timeSample;
          state.sampleStream = sample->sampleStream.get();
          pixelColor += rayCaster.rayColor(sample->ray, state);
        }
      }

      return pixelColor * sampleScale;
    }

    void writeColor(Buffer<Colord>& buffer, const Recti& rect,
                    const render::ViewPlane::Iterator& pixel, const Colord& color) const {
      const Recti footprint = pixel.footprintWithin(rect);
      for (int y = footprint.top(); y != footprint.bottom(); ++y)
        for (int x = footprint.left(); x != footprint.right(); ++x)
          buffer[y][x] = color;
    }

    void writeRGB(Buffer<unsigned int>& buffer, const Recti& rect,
                  const render::ViewPlane::Iterator& pixel, unsigned int rgb) const {
      const Recti footprint = pixel.footprintWithin(rect);
      for (int y = footprint.top(); y != footprint.bottom(); ++y)
        for (int x = footprint.left(); x != footprint.right(); ++x)
          buffer[y][x] = rgb;
    }

    void renderTile(render::Camera& camera, const render::RayCaster& rayCaster,
                    Buffer<Colord>& buffer, const Recti& rect,
                    std::optional<std::uint64_t> tileSeed) const {
      const Recti actualRect = camera.renderableRect(rect);
      if (actualRect.width() <= 0 || actualRect.height() <= 0)
        return;

      auto plane = camera.viewPlane();
      for (render::ViewPlane::Iterator pixel = plane->begin(actualRect),
                                       end = plane->end(actualRect);
           pixel != end; ++pixel) {
        if (camera.isCancelled())
          break;

        if (showProgressIndicators) {
          writeColor(buffer, actualRect, pixel, Colord(1, 0, 0));
        }

        const Colord averaged = tracePixel(camera, rayCaster, pixel, tileSeed);
        if (camera.isCancelled())
          break;

        writeColor(buffer, actualRect, pixel, averaged);
      }
    }

    void renderTile(render::Camera& camera, const render::RayCaster& rayCaster,
                    Buffer<unsigned int>& buffer, std::shared_ptr<render::Tonemap> tonemap,
                    const Recti& rect, std::optional<std::uint64_t> tileSeed) const {
      const Recti actualRect = camera.renderableRect(rect);
      if (actualRect.width() <= 0 || actualRect.height() <= 0)
        return;

      auto plane = camera.viewPlane();
      for (render::ViewPlane::Iterator pixel = plane->begin(actualRect),
                                       end = plane->end(actualRect);
           pixel != end; ++pixel) {
        if (camera.isCancelled())
          break;

        if (showProgressIndicators) {
          writeRGB(buffer, actualRect, pixel, 0xffff0000);
        }

        const Colord averaged = tracePixel(camera, rayCaster, pixel, tileSeed);
        if (camera.isCancelled())
          break;

        writeRGB(buffer, actualRect, pixel, (tonemap ? tonemap->apply(averaged) : averaged).rgb());
      }
    }

    void renderTile(render::Camera& camera, const render::RayCaster& rayCaster,
                    Buffer<Colord>& hdrBuffer, Buffer<unsigned int>& displayBuffer,
                    std::shared_ptr<render::Tonemap> tonemap, const Recti& rect,
                    std::optional<std::uint64_t> tileSeed) const {
      const Recti actualRect = camera.renderableRect(rect);
      if (actualRect.width() <= 0 || actualRect.height() <= 0)
        return;

      auto plane = camera.viewPlane();
      for (render::ViewPlane::Iterator pixel = plane->begin(actualRect),
                                       end = plane->end(actualRect);
           pixel != end; ++pixel) {
        if (camera.isCancelled())
          break;

        if (showProgressIndicators) {
          writeColor(hdrBuffer, actualRect, pixel, Colord(1, 0, 0));
          writeRGB(displayBuffer, actualRect, pixel, 0xffff0000);
        }

        const Colord averaged = tracePixel(camera, rayCaster, pixel, tileSeed);
        if (camera.isCancelled())
          break;

        writeColor(hdrBuffer, actualRect, pixel, averaged);
        writeRGB(displayBuffer, actualRect, pixel,
                 (tonemap ? tonemap->apply(averaged) : averaged).rgb());
      }
    }

    void configureIntegratorCancellation(const WavefrontRaytracer& owner) {
      integrator->setCancellationCallback(
        [&owner] { return owner.camera() && owner.camera()->isCancelled(); });
    }
  };

  WavefrontRaytracer::WavefrontRaytracer(std::shared_ptr<render::Scene> scene)
      : RenderEngine(std::move(scene)),
        p(std::make_unique<Private>()) {
    p->configureIntegratorCancellation(*this);
  }

  WavefrontRaytracer::WavefrontRaytracer(std::shared_ptr<render::Camera> camera,
                                         std::shared_ptr<render::Scene> scene)
      : RenderEngine(std::move(camera), std::move(scene)),
        p(std::make_unique<Private>()) {
    p->configureIntegratorCancellation(*this);
  }

  WavefrontRaytracer::~WavefrontRaytracer() = default;

  std::shared_ptr<render::RenderEngine> WavefrontRaytracer::cloneForRender() const {
    auto result =
      std::make_shared<WavefrontRaytracer>(m_camera ? m_camera->clone() : nullptr, m_scene);
    result->setTonemap(tonemap());
    result->setIntegrator(p->integrator->clone());
    result->setMaximumThreads(p->threadPool->maxThreadCount());
    result->setQueueSize(p->queueSize);
    result->setShowProgressIndicators(p->showProgressIndicators);
    if (p->samplingSeed) {
      result->setSamplingSeed(*p->samplingSeed);
    }
    return result;
  }

  void WavefrontRaytracer::render(Buffer<Colord>& buffer) {
    if (!m_scene || !m_camera) {
      buffer.clear();
      return;
    }

    p->tasks.clear();

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().reset();
#endif

    m_camera->viewPlane()->setup(m_camera->matrix(), buffer.rect());
    m_camera->setShowProgressIndicators(p->showProgressIndicators);

    auto rayCaster = std::make_shared<RecursiveRayCasterAdapter>(*m_scene, *p->integrator);
    auto camera = m_camera;
    Buffer<Colord>* bufferPtr = &buffer;

    const render::TilePlan tilePlan =
      render::TilePlan::forBuffer(buffer.width(), buffer.height(), p->queueSize);
    const auto samplingSeed = p->samplingSeed;
    engine::dispatchTileTasks(
      tilePlan, *p->threadPool, p->tasks,
      [this, rayCaster, camera, bufferPtr, samplingSeed](const Recti& rect, std::size_t tileIndex) {
        const std::optional<std::uint64_t> tileSeed =
          samplingSeed
            ? std::optional<std::uint64_t>(render::SamplingSeed::tileSeed(*samplingSeed, tileIndex))
            : std::nullopt;
        p->renderTile(*camera, *rayCaster, *bufferPtr, rect, tileSeed);
      });

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().dumpJson(std::cerr);
#endif
  }

  void WavefrontRaytracer::render(Buffer<unsigned int>& buffer) {
    if (!m_scene || !m_camera) {
      buffer.clear();
      return;
    }

    p->tasks.clear();

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().reset();
#endif

    m_camera->viewPlane()->setup(m_camera->matrix(), buffer.rect());
    m_camera->setShowProgressIndicators(p->showProgressIndicators);

    auto rayCaster = std::make_shared<RecursiveRayCasterAdapter>(*m_scene, *p->integrator);
    auto camera = m_camera;
    auto tonemapOp = tonemap();
    Buffer<unsigned int>* bufferPtr = &buffer;

    const render::TilePlan tilePlan =
      render::TilePlan::forBuffer(buffer.width(), buffer.height(), p->queueSize);
    const auto samplingSeed = p->samplingSeed;
    engine::dispatchTileTasks(
      tilePlan, *p->threadPool, p->tasks,
      [this, rayCaster, camera, bufferPtr, tonemapOp, samplingSeed](const Recti& rect,
                                                                    std::size_t tileIndex) {
        const std::optional<std::uint64_t> tileSeed =
          samplingSeed
            ? std::optional<std::uint64_t>(render::SamplingSeed::tileSeed(*samplingSeed, tileIndex))
            : std::nullopt;
        p->renderTile(*camera, *rayCaster, *bufferPtr, tonemapOp, rect, tileSeed);
      });

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().dumpJson(std::cerr);
#endif
  }

  void WavefrontRaytracer::render(Buffer<Colord>& hdrBuffer, Buffer<unsigned int>& displayBuffer,
                                  std::shared_ptr<render::Tonemap> displayTonemap) {
    if (!core::util::bufferDimensionsEqual(hdrBuffer, displayBuffer)) {
      throw std::runtime_error("wavefront dual-output render requires matching buffer dimensions");
    }

    if (!m_scene || !m_camera) {
      hdrBuffer.clear();
      displayBuffer.clear();
      return;
    }

    p->tasks.clear();

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().reset();
#endif

    m_camera->viewPlane()->setup(m_camera->matrix(), hdrBuffer.rect());
    m_camera->setShowProgressIndicators(p->showProgressIndicators);

    auto rayCaster = std::make_shared<RecursiveRayCasterAdapter>(*m_scene, *p->integrator);
    auto camera = m_camera;
    Buffer<Colord>* hdrBufferPtr = &hdrBuffer;
    Buffer<unsigned int>* displayBufferPtr = &displayBuffer;

    const render::TilePlan tilePlan =
      render::TilePlan::forBuffer(hdrBuffer.width(), hdrBuffer.height(), p->queueSize);
    const auto samplingSeed = p->samplingSeed;
    engine::dispatchTileTasks(
      tilePlan, *p->threadPool, p->tasks,
      [this, rayCaster, camera, hdrBufferPtr, displayBufferPtr, displayTonemap,
       samplingSeed](const Recti& rect, std::size_t tileIndex) {
        const std::optional<std::uint64_t> tileSeed =
          samplingSeed
            ? std::optional<std::uint64_t>(render::SamplingSeed::tileSeed(*samplingSeed, tileIndex))
            : std::nullopt;
        p->renderTile(*camera, *rayCaster, *hdrBufferPtr, *displayBufferPtr, displayTonemap, rect,
                      tileSeed);
      });

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().dumpJson(std::cerr);
#endif
  }

  void WavefrontRaytracer::cancel() {
    if (m_camera) {
      m_camera->cancel();
    }
  }

  void WavefrontRaytracer::uncancel() {
    if (m_camera) {
      m_camera->uncancel();
    }
  }

  std::list<Recti> WavefrontRaytracer::activeTiles() const {
    std::list<Recti> result;
    for (const auto& task : p->tasks) {
      if (task->active.load(std::memory_order_acquire)) {
        result.push_back(task->rect);
      }
    }
    return result;
  }

  std::list<Recti> WavefrontRaytracer::completedTiles() const {
    std::list<Recti> result;
    for (const auto& task : p->tasks) {
      if (task->completed.load(std::memory_order_acquire)) {
        result.push_back(task->rect);
      }
    }
    return result;
  }

  void WavefrontRaytracer::setIntegrator(std::unique_ptr<render::Integrator> integrator) {
    if (!integrator) {
      throw std::invalid_argument("WavefrontRaytracer integrator cannot be null");
    }
    p->integrator = std::move(integrator);
    p->configureIntegratorCancellation(*this);
  }

  const render::Integrator& WavefrontRaytracer::integrator() const {
    return *p->integrator;
  }

  void WavefrontRaytracer::setMaximumRecursionDepth(int depth) {
    p->integrator->setMaximumRecursionDepth(depth);
  }

  void WavefrontRaytracer::setSamplingSeed(std::uint64_t seed) {
    p->samplingSeed = seed;
  }

  void WavefrontRaytracer::clearSamplingSeed() {
    p->samplingSeed.reset();
  }

  std::optional<std::uint64_t> WavefrontRaytracer::samplingSeed() const {
    return p->samplingSeed;
  }

  void WavefrontRaytracer::setMaximumThreads(int threads) {
    p->threadPool->setMaxThreadCount(threads);
  }

  void WavefrontRaytracer::setQueueSize(int queue) {
    p->queueSize = queue;
  }

  void WavefrontRaytracer::setShowProgressIndicators(bool show) {
    p->showProgressIndicators = show;
  }
}
