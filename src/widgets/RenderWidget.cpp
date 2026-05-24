#include "widgets/RenderWidget.h"
#include "render/RenderEngine.h"
#include "render/cameras/Camera.h"
#include "core/Buffer.h"

#include <QImage>
#include <QPainter>
#include <QThread>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <list>
#include <utility>

using namespace std;
using namespace render;

namespace {
  class RenderThread : public QThread {
  public:
    inline RenderThread(std::shared_ptr<render::RenderEngine> e,
                        std::shared_ptr<Buffer<unsigned int>> b)
        : engine(std::move(e)),
          buffer(std::move(b)) {
    }

    inline virtual void run() {
      engine->render(*buffer);
    }

    inline void cancel() {
      engine->cancel();
    }

    std::shared_ptr<render::RenderEngine> engine;
    std::shared_ptr<Buffer<unsigned int>> buffer;
  };

  struct RenderJob {
    RenderJob(std::uint64_t generation, std::shared_ptr<render::RenderEngine> engine,
              std::shared_ptr<Buffer<unsigned int>> buffer, bool isolated)
        : generation(generation),
          thread(new RenderThread(std::move(engine), std::move(buffer))),
          discardFinishedFrame(false),
          isolated(isolated) {
    }

    ~RenderJob() {
      delete thread;
    }

    std::uint64_t generation;
    RenderThread* thread;
    bool discardFinishedFrame;
    bool isolated;
  };
}

struct RenderWidget::Private {
  inline Private()
      : timer(0),
        showProgressIndicators(false),
        displayMode(DisplayMode::PeriodicUpdate),
        clearBackBufferOnRenderStart(true),
        progressUpdateIntervalMs(0),
        renderGeneration(0),
        aspectMode(render::AspectMode::FitWidth),
        aspectRatio(0.0) {
  }

  std::unique_ptr<RenderJob> activeJob;
  std::list<std::unique_ptr<RenderJob>> retiredJobs;
  QSize bufferSize;
  QImage frontImage;
  int timer;
  bool showProgressIndicators;
  DisplayMode displayMode;
  bool clearBackBufferOnRenderStart;
  int progressUpdateIntervalMs;
  std::uint64_t renderGeneration;
  render::AspectMode aspectMode;
  double aspectRatio;
};

RenderWidget::RenderWidget(QWidget* parent, std::shared_ptr<render::RenderEngine> engine)
    : QWidget(parent),
      m_engine(std::move(engine)),
      p(std::make_unique<Private>()) {
  setBufferSize(QSize(0, 0));
}

void RenderWidget::setEngine(std::shared_ptr<render::RenderEngine> engine) {
  m_engine = std::move(engine);
}

std::shared_ptr<render::RenderEngine> RenderWidget::renderEngine() const {
  return m_engine;
}

RenderWidget::~RenderWidget() {
  stop();
}

void RenderWidget::stop() {
  if (p->activeJob && !p->activeJob->discardFinishedFrame) {
    publishProgressUpdate();
  }
  const bool wasRunning = p->activeJob && p->activeJob->thread->isRunning();
  if (p->activeJob && p->activeJob->thread->isRunning()) {
    p->activeJob->discardFinishedFrame = true;
    p->activeJob->thread->cancel();
    p->activeJob->thread->wait();
  }

  if (p->activeJob && !wasRunning && !p->activeJob->discardFinishedFrame) {
    publishFullBackBuffer();
  } else if (p->activeJob && !p->activeJob->discardFinishedFrame) {
    publishProgressUpdate();
  }

  if (p->activeJob) {
    disconnect(p->activeJob->thread, nullptr, this, nullptr);
    p->activeJob.reset();
    ++p->renderGeneration;
  }

  for (auto& job : p->retiredJobs) {
    if (job->thread->isRunning()) {
      job->thread->cancel();
      job->thread->wait();
    }
    disconnect(job->thread, nullptr, this, nullptr);
  }
  p->retiredJobs.clear();

  if (p->timer != 0) {
    killTimer(p->timer);
    p->timer = 0;
  }
  update();
}

void RenderWidget::render() {
  reapRetiredRenderJobs();

  // Apply stored aspect settings to the control engine's camera before
  // cloneForRender() captures a snapshot, so the clone inherits them.
  if (m_engine && m_engine->camera()) {
    m_engine->camera()->setAspectMode(p->aspectMode);
    m_engine->camera()->setAspectRatio(p->aspectRatio);
  }

  if (p->activeJob && p->activeJob->thread->isRunning() && p->activeJob->isolated &&
      !p->retiredJobs.empty()) {
    // Backpressure before cloning: the next snapshot would allocate a
    // fresh engine, buffer, and worker while an older cancellation is
    // still draining. Cancel the active frame and let the caller
    // coalesce to a later render request.
    cancelActiveRender();
    return;
  }

  auto renderEngine = m_engine->cloneForRender();
  const bool isolatedRender = static_cast<bool>(renderEngine);
  if (!renderEngine) {
    renderEngine = m_engine;
  }

  if (p->activeJob) {
    if (p->activeJob->thread->isRunning()) {
      if (p->activeJob->isolated && isolatedRender) {
        if (p->retiredJobs.empty()) {
          retireActiveRender();
        } else {
          // One retired snapshot is already draining. Keep the
          // current job as the active cancellation fence instead of
          // spawning unbounded render pools while the user drags.
          cancelActiveRender();
          return;
        }
      } else {
        stop();
      }
    } else {
      clearInactiveActiveRender();
    }
  }

  m_engine->uncancel();
  renderEngine->uncancel();

  auto buffer =
    std::make_shared<Buffer<unsigned int>>(p->bufferSize.width(), p->bufferSize.height());
  if (p->clearBackBufferOnRenderStart) {
    buffer->clear();
  } else {
    copyFrontImageTo(*buffer);
  }

  const std::uint64_t generation = ++p->renderGeneration;
  p->activeJob = std::make_unique<RenderJob>(generation, renderEngine, buffer, isolatedRender);
  connect(p->activeJob->thread, &QThread::finished, this,
          [this, generation]() { renderThreadDone(generation); });
  p->activeJob->thread->start();

  if (p->displayMode != DisplayMode::DoubleBuffer || p->showProgressIndicators) {
    const int interval = p->progressUpdateIntervalMs > 0 ? p->progressUpdateIntervalMs
                                                         : std::max(16, p->bufferSize.width() / 10);
    p->timer = startTimer(interval);
  }
}

void RenderWidget::setBufferSize(const QSize& size) {
  p->bufferSize = size;
  p->frontImage = QImage(size.width(), size.height(), QImage::Format_RGB32);
  p->frontImage.fill(Qt::black);
}

QSize RenderWidget::bufferSize() const {
  return p->bufferSize;
}

void RenderWidget::setShowProgressIndicators(bool show) {
  p->showProgressIndicators = show;
}

void RenderWidget::setDisplayMode(DisplayMode mode) {
  p->displayMode = mode;
}

RenderWidget::DisplayMode RenderWidget::displayMode() const {
  return p->displayMode;
}

void RenderWidget::setClearBackBufferOnRenderStart(bool clear) {
  p->clearBackBufferOnRenderStart = clear;
}

void RenderWidget::setProgressUpdateIntervalMs(int intervalMs) {
  p->progressUpdateIntervalMs = std::max(0, intervalMs);
}

void RenderWidget::setAspectMode(render::AspectMode mode) {
  p->aspectMode = mode;
}

render::AspectMode RenderWidget::aspectMode() const {
  return p->aspectMode;
}

void RenderWidget::setAspectRatio(double ratio) {
  if (ratio > 0.0) {
    p->aspectMode = render::AspectMode::FitExact;
    p->aspectRatio = ratio;
  } else {
    p->aspectMode = render::AspectMode::FitWidth;
    p->aspectRatio = 0.0;
  }
}

bool RenderWidget::isRendering() const {
  return p->activeJob && p->activeJob->thread->isRunning();
}

void RenderWidget::cancelRender() {
  if (!p->activeJob || !p->activeJob->thread->isRunning())
    return;

  reapRetiredRenderJobs();

  if (!p->activeJob->isolated || !p->retiredJobs.empty()) {
    cancelActiveRender();
    return;
  }

  retireActiveRender();
}

void RenderWidget::cancelActiveRender() {
  if (!p->activeJob)
    return;

  p->activeJob->discardFinishedFrame = true;
  if (p->timer != 0) {
    killTimer(p->timer);
    p->timer = 0;
  }
  p->activeJob->thread->cancel();
  update();
}

void RenderWidget::clearInactiveActiveRender() {
  if (!p->activeJob || p->activeJob->thread->isRunning())
    return;

  if (!p->activeJob->discardFinishedFrame) {
    publishFullBackBuffer();
  }
  disconnect(p->activeJob->thread, nullptr, this, nullptr);
  p->activeJob.reset();
  ++p->renderGeneration;
}

void RenderWidget::reapRetiredRenderJobs() {
  for (auto job = p->retiredJobs.begin(); job != p->retiredJobs.end();) {
    if ((*job)->thread->isRunning()) {
      ++job;
      continue;
    }
    disconnect((*job)->thread, nullptr, this, nullptr);
    job = p->retiredJobs.erase(job);
  }
}

void RenderWidget::retireActiveRender() {
  if (!p->activeJob)
    return;

  cancelActiveRender();
  p->retiredJobs.push_back(std::move(p->activeJob));
  update();
}

void RenderWidget::timerEvent(QTimerEvent*) {
  if (p->activeJob && !p->activeJob->discardFinishedFrame) {
    publishProgressUpdate();
  }
  update();
}

void RenderWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);

  if (p->activeJob && p->activeJob->thread->isRunning() && p->showProgressIndicators) {
    QImage image = p->frontImage.copy();
    markTilesInProgress(image);
    painter.drawImage(QPoint(0, 0), image);
    return;
  }

  painter.drawImage(QPoint(0, 0), p->frontImage);
}

Buffer<unsigned int>* RenderWidget::activeBackBuffer() const {
  if (!p->activeJob)
    return nullptr;
  return p->activeJob->thread->buffer.get();
}

render::RenderEngine* RenderWidget::activeRenderEngine() const {
  if (!p->activeJob)
    return nullptr;
  return p->activeJob->thread->engine.get();
}

void RenderWidget::copyFrontImageTo(Buffer<unsigned int>& buffer) const {
  for (int y = 0; y < buffer.height(); ++y) {
    std::memcpy(buffer[y], p->frontImage.constScanLine(y), sizeof(unsigned int) * buffer.width());
  }
}

void RenderWidget::publishProgressUpdate() {
  switch (p->displayMode) {
  case DisplayMode::PeriodicUpdate:
    publishFullBackBuffer();
    break;
  case DisplayMode::CompletedTilePublishing:
    publishCompletedTiles();
    break;
  case DisplayMode::DoubleBuffer:
    break;
  }
}

void RenderWidget::publishCompletedTiles() {
  auto* engine = activeRenderEngine();
  if (!engine)
    return;

  for (const auto& tile : engine->completedTiles()) {
    publishTile(tile);
  }
}

void RenderWidget::publishFullBackBuffer() {
  auto* buffer = activeBackBuffer();
  if (!buffer)
    return;

  publishTile(Recti(0, 0, buffer->width(), buffer->height()));
}

void RenderWidget::publishTile(const Recti& tile) {
  auto* buffer = activeBackBuffer();
  if (!buffer)
    return;

  const int left = std::max(0, tile.left());
  const int top = std::max(0, tile.top());
  const int right = std::min(buffer->width(), tile.right());
  const int bottom = std::min(buffer->height(), tile.bottom());

  for (int x = left; x < right; ++x) {
    for (int y = top; y < bottom; ++y) {
      p->frontImage.setPixel(x, y, (*buffer)[y][x]);
    }
  }
}

void RenderWidget::markTilesInProgress(QImage& image) const {
  auto* engine = activeRenderEngine();
  if (!engine)
    return;

  for (const auto& tile : engine->activeTiles()) {
    const int left = std::max(0, tile.left());
    const int top = std::max(0, tile.top());
    const int right = std::min(image.width(), tile.right());
    const int bottom = std::min(image.height(), tile.bottom());
    for (int x = left; x != right; ++x) {
      for (int y = top; y != bottom; ++y) {
        image.setPixel(x, y, progressTint(image.pixel(x, y)));
      }
    }
  }
}

QRgb RenderWidget::progressTint(QRgb color) const {
  return qRgb(std::min(255, qRed(color) + 64), qGreen(color) * 0.75, qBlue(color) * 0.75);
}

void RenderWidget::renderThreadDone(std::uint64_t generation) {
  if (!p->activeJob || generation != p->activeJob->generation) {
    auto retired =
      std::find_if(p->retiredJobs.begin(), p->retiredJobs.end(),
                   [generation](const auto& job) { return job->generation == generation; });
    if (retired != p->retiredJobs.end()) {
      disconnect((*retired)->thread, nullptr, this, nullptr);
      p->retiredJobs.erase(retired);
    }
    return;
  }

  if (!p->activeJob->discardFinishedFrame) {
    publishFullBackBuffer();
  }
  if (p->timer != 0) {
    killTimer(p->timer);
    p->timer = 0;
  }
  update();
  emit finished();
}
