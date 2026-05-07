#include "raytracer/Raytracer.h"
#include "raytracer/State.h"
#include "render/Stats.h"
#include "core/math/Vector.h"
#include "core/math/Ray.h"
#include "raytracer/primitives/Scene.h"
#include "core/Buffer.h"
#include "core/math/HitPoint.h"
#include "core/math/HitPointInterval.h"
#include "raytracer/materials/Material.h"
#include "core/math/Matrix.h"
#include "core/math/Rect.h"
#include "core/math/IntegerDecomposition.h"
#include "raytracer/cameras/Camera.h"
#include "render/tonemap/Tonemap.h"
#include "core/Exception.h"
#include "core/ScopeExit.h"

#include <QThreadPool>
#include <QRunnable>

#include <atomic>
#include <functional>
#include <vector>
#include <cmath>

#include <iostream>

using namespace std;
using namespace raytracer;

namespace {
  // Tile-render task — generic over what the per-tile work is so the
  // same task type can drive both the HDR (`Buffer<Colord>&`) and LDR
  // (`Buffer<unsigned int>&` + tonemap) render paths. Both paths
  // need the same activeRects bookkeeping, so duplicating the task
  // class would just split that logic without buying anything.
  class RenderTask : public QRunnable {
  public:
    inline RenderTask(const Recti& r, std::function<void()> w)
      : QRunnable(),
        active(false),
        rect(r),
        work(std::move(w))
    {
      setAutoDelete(false);
    }

    inline virtual void run() {
      try {
        active = true;
        work();
      } catch(Exception& e) {
        e.printBacktrace();
      }
      active = false;
    }

    // Written by the worker thread that runs this task and read by the main
    // thread in Raytracer::activeRects(). std::atomic<bool> gives a defined
    // happens-before relationship with sequential consistency on load/store
    // (the default), avoiding the data race that the previous plain bool had.
    std::atomic<bool> active;
    Recti rect;

  private:
    std::function<void()> work;
  };
}

struct Raytracer::Private {
  inline Private()
    : threadPool(std::make_unique<QThreadPool>()),
      queueSize(QThread::idealThreadCount()),
      maximumRecursionDepth(10),
      showProgressIndicators(false)
  {
  }

  std::unique_ptr<QThreadPool> threadPool;
  list<shared_ptr<RenderTask>> tasks;
  int queueSize;
  int maximumRecursionDepth;
  bool showProgressIndicators;
};

Raytracer::Raytracer(std::shared_ptr<Scene> scene)
  : RenderEngine(std::move(scene)),
    p(std::make_unique<Private>())
{
}

Raytracer::Raytracer(std::shared_ptr<Camera> camera, std::shared_ptr<Scene> scene)
  : RenderEngine(std::move(camera), std::move(scene)),
    p(std::make_unique<Private>())
{
}

Raytracer::~Raytracer() {
}

namespace {
  // Per-tile rect for an `IntegerDecomposition` cell. Same math used
  // by both the HDR and LDR dispatch paths below — pulling it out
  // keeps the two paths from drifting.
  Recti tileRect(int width, int height, int rows, int cols, int rowIdx, int colIdx) {
    return Recti(
      floor(double(width)  / cols * colIdx),
      floor(double(height) / rows * rowIdx),
      ceil (double(width)  / cols),
      ceil (double(height) / rows)
    );
  }
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

  // shared_from_this() returns shared_ptr<RenderEngine>; the
  // RenderTask + Camera::render API both want shared_ptr<Raytracer>
  // because the workers call rayColor() (a Raytracer-specific
  // method). Static-cast is safe — `this` is definitively a
  // Raytracer in our own member function.
  auto self = std::static_pointer_cast<Raytracer>(shared_from_this());
  auto camera = m_camera;
  Buffer<Colord>* bufferPtr = &buffer;

  IntegerDecomposition d(p->queueSize);
  for (int vert = 0; vert != d.first(); ++vert) {
    for (int horz = 0; horz != d.second(); ++horz) {
      Recti rect = tileRect(buffer.width(), buffer.height(), d.first(), d.second(), vert, horz);
      auto task = std::make_shared<RenderTask>(rect, [self, camera, bufferPtr, rect] {
        camera->render(self, *bufferPtr, rect);
      });

      p->tasks.push_back(task);
      p->threadPool->start(task.get());
    }
  }

  p->threadPool->waitForDone();

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

  IntegerDecomposition d(p->queueSize);
  for (int vert = 0; vert != d.first(); ++vert) {
    for (int horz = 0; horz != d.second(); ++horz) {
      Recti rect = tileRect(buffer.width(), buffer.height(), d.first(), d.second(), vert, horz);
      auto task = std::make_shared<RenderTask>(rect, [self, camera, bufferPtr, tonemapOp, rect] {
        camera->render(self, *bufferPtr, tonemapOp, rect);
      });

      p->tasks.push_back(task);
      p->threadPool->start(task.get());
    }
  }

  p->threadPool->waitForDone();

#ifdef RAYTRACER_ENABLE_STATS
  ::render::stats::Counters::instance().dumpJson(std::cerr);
#endif
}

const Primitive* Raytracer::primitiveForRay(const Rayd& ray) const {
  return rayState(ray).hitPoint.primitive();
}

State Raytracer::rayState(const Rayd& ray) const {
  State state;
  state.startTrace();
  rayColor(ray, state);
  return state;
}

Colord Raytracer::rayColor(const Rayd& ray, State& state) const {
  state.recurseIn();
  ScopeExit sx([&] { state.recurseOut(); });

  if (state.recursionDepth == p->maximumRecursionDepth) {
    state.recordEvent(nullptr, "Raytracer: maximum recursion depth reached, returning background");
    return m_scene->background();
  }

  HitPointInterval hitPoints;

  auto primitive = m_scene->intersect(ray, hitPoints, state);
  if (primitive) {
    auto hitPoint = hitPoints.minWithPositiveDistance();

    if (state.recursionDepth == 1) {
      state.hitPoint = hitPoint;
    }

    if (primitive->material()) {
      state.recordEvent(nullptr, "Raytracer: shading material");
      return primitive->material()->shade(this, ray, hitPoint, state);
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

std::list<Recti> Raytracer::activeRects() const {
  std::list<Recti> result;
  for (const auto& task : p->tasks) {
    if (task->active) {
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
