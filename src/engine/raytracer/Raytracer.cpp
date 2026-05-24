#include "engine/raytracer/Raytracer.h"
#include "render/State.h"
#include "render/Stats.h"
#include "core/math/Constants.h"
#include "core/math/Vector.h"
#include "core/math/Ray.h"
#include "render/primitives/Scene.h"
#include "core/Buffer.h"
#include "core/math/HitPoint.h"
#include "core/math/HitPointInterval.h"
#include "render/materials/Material.h"
#include "core/math/Matrix.h"
#include "core/math/Rect.h"
#include "render/TilePlan.h"
#include "render/cameras/Camera.h"
#include "render/tonemap/Tonemap.h"
#include "core/ScopeExit.h"

#include "../TileRenderTask.h"

#include <QThread>
#include <QThreadPool>

#include <iostream>
#include <stdexcept>

using namespace std;
using namespace engine::raytracer;

struct Raytracer::Private {
  inline Private()
      : threadPool(std::make_unique<QThreadPool>()),
        queueSize(QThread::idealThreadCount()),
        maximumRecursionDepth(10),
        showProgressIndicators(false) {
  }

  std::unique_ptr<QThreadPool> threadPool;
  list<shared_ptr<engine::TileRenderTask>> tasks;
  int queueSize;
  int maximumRecursionDepth;
  bool showProgressIndicators;
};

Raytracer::Raytracer(std::shared_ptr<render::Scene> scene)
    : RenderEngine(std::move(scene)),
      p(std::make_unique<Private>()) {
}

Raytracer::Raytracer(std::shared_ptr<render::Camera> camera, std::shared_ptr<render::Scene> scene)
    : RenderEngine(std::move(camera), std::move(scene)),
      p(std::make_unique<Private>()) {
}

Raytracer::~Raytracer() {
}

std::shared_ptr<render::RenderEngine> Raytracer::cloneForRender() const {
  auto result = std::make_shared<Raytracer>(m_camera ? m_camera->clone() : nullptr, m_scene);
  result->setTonemap(tonemap());
  result->setMaximumRecursionDepth(p->maximumRecursionDepth);
  result->setMaximumThreads(p->threadPool->maxThreadCount());
  result->setQueueSize(p->queueSize);
  result->setShowProgressIndicators(p->showProgressIndicators);
  return result;
}

void Raytracer::render(Buffer<Colord>& buffer) {
  if (!m_scene) {
    buffer.clear();
    return;
  }

  p->tasks.clear();

#ifdef RAYTRACER_ENABLE_STATS
  ::render::stats::Counters::instance().reset();
#endif

  m_camera->viewPlane()->setup(m_camera->matrix(), buffer.rect());
  m_camera->setShowProgressIndicators(p->showProgressIndicators);

  // shared_from_this() returns shared_ptr<render::RenderEngine>; the
  // TileRenderTask + Camera::render API both want shared_ptr<Raytracer>
  // because the workers call rayColor() (a Raytracer-specific
  // method). Static-cast is safe — `this` is definitively a
  // Raytracer in our own member function.
  auto self = std::static_pointer_cast<Raytracer>(shared_from_this());
  auto camera = m_camera;
  Buffer<Colord>* bufferPtr = &buffer;

  const render::TilePlan tilePlan =
    render::TilePlan::forBuffer(buffer.width(), buffer.height(), p->queueSize);
  engine::dispatchTileTasks(tilePlan, *p->threadPool, p->tasks,
                            [self, camera, bufferPtr](const Recti& rect, std::size_t) {
                              camera->render(self, *bufferPtr, rect);
                            });

#ifdef RAYTRACER_ENABLE_STATS
  // Sampling counters after waitForDone() returns means all worker writes are
  // already visible; relaxed loads in dumpJson() are sufficient.
  ::render::stats::Counters::instance().dumpJson(std::cerr);
#endif
}

void Raytracer::render(Buffer<unsigned int>& buffer) {
  // LDR dispatch — workers tonemap and write packed RGB inline so an
  // interactive widget polling the buffer sees progressive output.
  // Mirrors the HDR dispatch above; the only differences are the
  // buffer type passed to the worker and the tonemap argument.
  if (!m_scene) {
    buffer.clear();
    return;
  }

  p->tasks.clear();

#ifdef RAYTRACER_ENABLE_STATS
  ::render::stats::Counters::instance().reset();
#endif

  m_camera->viewPlane()->setup(m_camera->matrix(), buffer.rect());
  m_camera->setShowProgressIndicators(p->showProgressIndicators);

  auto self = std::static_pointer_cast<Raytracer>(shared_from_this());
  auto camera = m_camera;
  auto tonemapOp = tonemap();
  Buffer<unsigned int>* bufferPtr = &buffer;

  const render::TilePlan tilePlan =
    render::TilePlan::forBuffer(buffer.width(), buffer.height(), p->queueSize);
  engine::dispatchTileTasks(tilePlan, *p->threadPool, p->tasks,
                            [self, camera, bufferPtr, tonemapOp](const Recti& rect, std::size_t) {
                              camera->render(self, *bufferPtr, tonemapOp, rect);
                            });

#ifdef RAYTRACER_ENABLE_STATS
  ::render::stats::Counters::instance().dumpJson(std::cerr);
#endif
}

void Raytracer::render(Buffer<Colord>& hdrBuffer, Buffer<unsigned int>& displayBuffer,
                       std::shared_ptr<render::Tonemap> displayTonemap) {
  if (hdrBuffer.width() != displayBuffer.width() || hdrBuffer.height() != displayBuffer.height()) {
    throw std::runtime_error("raytracer dual-output render requires matching buffer dimensions");
  }

  if (!m_scene) {
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

  auto self = std::static_pointer_cast<Raytracer>(shared_from_this());
  auto camera = m_camera;
  Buffer<Colord>* hdrBufferPtr = &hdrBuffer;
  Buffer<unsigned int>* displayBufferPtr = &displayBuffer;

  const render::TilePlan tilePlan =
    render::TilePlan::forBuffer(hdrBuffer.width(), hdrBuffer.height(), p->queueSize);
  engine::dispatchTileTasks(
    tilePlan, *p->threadPool, p->tasks,
    [self, camera, hdrBufferPtr, displayBufferPtr, displayTonemap](const Recti& rect, std::size_t) {
      camera->render(self, *hdrBufferPtr, *displayBufferPtr, displayTonemap, rect);
    });

#ifdef RAYTRACER_ENABLE_STATS
  ::render::stats::Counters::instance().dumpJson(std::cerr);
#endif
}

const render::Primitive* Raytracer::primitiveForRay(const Rayd& ray) const {
  return rayState(ray).hitPoint.primitive();
}

render::State Raytracer::rayState(const Rayd& ray) const {
  render::State state;
  state.startTrace();
  rayColor(ray, state);
  return state;
}

Colord Raytracer::rayColor(const Rayd& ray, render::State& state) const {
  if (m_camera->isCancelled())
    return m_scene ? m_scene->background() : Colord::black();

  state.recurseIn();
  ScopeExit sx([&] { state.recurseOut(); });

  if (state.recursionDepth == p->maximumRecursionDepth) {
    state.recordEvent(nullptr, "Raytracer: maximum recursion depth reached, returning background");
    return m_scene->background();
  }

  if (state.throughput < RAYTRACER_THROUGHPUT_CUTOFF) {
    state.recordEvent(nullptr, "Raytracer: throughput below cutoff, returning background");
    return m_scene->background();
  }

  HitPointInterval hitPoints;

  auto primitive = m_scene->intersect(ray, hitPoints, state);
  if (m_camera->isCancelled())
    return m_scene->background();

  if (primitive) {
    auto hitPoint = hitPoints.minWithPositiveDistance();

    if (state.recursionDepth == 1) {
      state.hitPoint = hitPoint;
    }

    if (primitive->material()) {
      state.recordEvent(nullptr, "Raytracer: shading material");
      return primitive->material()->shade(this, *m_scene, ray, hitPoint, state);
    } else {
      state.recordEvent(nullptr, "Raytracer: no material found, returning black");
      return Colord::black();
    }
  } else {
    state.recordEvent(nullptr, "Raytracer: Nothing hit, returning background color");
    return m_scene->background();
  }
}

void Raytracer::cancel() {
  m_camera->cancel();
}

void Raytracer::uncancel() {
  m_camera->uncancel();
}

std::list<Recti> Raytracer::activeTiles() const {
  std::list<Recti> result;
  for (const auto& task : p->tasks) {
    if (task->active.load(std::memory_order_acquire)) {
      result.push_back(task->rect);
    }
  }
  return result;
}

std::list<Recti> Raytracer::completedTiles() const {
  std::list<Recti> result;
  for (const auto& task : p->tasks) {
    if (task->completed.load(std::memory_order_acquire)) {
      result.push_back(task->rect);
    }
  }
  return result;
}

void Raytracer::setMaximumRecursionDepth(int depth) {
  p->maximumRecursionDepth = depth;
}

void Raytracer::setMaximumThreads(int threads) {
  p->threadPool->setMaxThreadCount(threads);
}

void Raytracer::setQueueSize(int queue) {
  p->queueSize = queue;
}

void Raytracer::setShowProgressIndicators(bool show) {
  p->showProgressIndicators = show;
}
