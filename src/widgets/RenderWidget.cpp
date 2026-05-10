#include "widgets/RenderWidget.h"
#include "render/RenderEngine.h"
#include "core/Buffer.h"

#include <QImage>
#include <QPainter>
#include <QThread>

#include <algorithm>

using namespace std;
using namespace render;

namespace {
  class RenderThread : public QThread {
  public:
    inline RenderThread(std::shared_ptr<render::RenderEngine> e, Buffer<unsigned int>& b)
      : engine(e),
        buffer(b)
    {
    }

    inline virtual void run() {
      engine->render(buffer);
    }

    inline void cancel() {
      engine->cancel();
    }

    std::shared_ptr<render::RenderEngine> engine;
    Buffer<unsigned int>& buffer;
  };
}

struct RenderWidget::Private {
  inline Private()
    : renderThread(nullptr),
      timer(0),
      showProgressIndicators(false)
  {
  }

  RenderThread* renderThread;

  std::unique_ptr<Buffer<unsigned int>> backBuffer;
  QImage frontImage;
  int timer;
  bool showProgressIndicators;
};

RenderWidget::RenderWidget(QWidget* parent, std::shared_ptr<render::RenderEngine> engine)
  : QWidget(parent),
    m_engine(std::move(engine)),
    p(std::make_unique<Private>())
{
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
  publishCompletedTiles();
  const bool wasRunning = p->renderThread && p->renderThread->isRunning();
  if (p->renderThread && p->renderThread->isRunning()) {
    p->renderThread->cancel();
    p->renderThread->wait();
  }

  if (p->renderThread && !wasRunning) {
    publishFullBackBuffer();
  } else {
    publishCompletedTiles();
  }

  if (p->renderThread) {
    delete p->renderThread;
    p->renderThread = nullptr;
  }
  if (p->timer != 0) {
    killTimer(p->timer);
    p->timer = 0;
  }
  update();
}

void RenderWidget::render() {
  stop();
  m_engine->uncancel();
  p->backBuffer->clear();
  p->frontImage.fill(Qt::black);

  p->renderThread = new RenderThread(m_engine, *p->backBuffer);
  connect(p->renderThread, SIGNAL(finished()), this, SLOT(renderThreadDone()));
  p->renderThread->start();

  const int interval = std::max(16, p->backBuffer->width() / 10);
  p->timer = startTimer(interval);
}

void RenderWidget::setBufferSize(const QSize& size) {
  p->backBuffer = std::make_unique<Buffer<unsigned int>>(size.width(), size.height());
  p->backBuffer->clear();
  p->frontImage = QImage(size.width(), size.height(), QImage::Format_RGB32);
  p->frontImage.fill(Qt::black);
}

void RenderWidget::setShowProgressIndicators(bool show) {
  p->showProgressIndicators = show;
}

void RenderWidget::timerEvent(QTimerEvent*) {
  publishCompletedTiles();
  update();
}

void RenderWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);

  if (p->renderThread && p->renderThread->isRunning() && p->showProgressIndicators) {
    QImage image = p->frontImage.copy();
    markTilesInProgress(image);
    painter.drawImage(QPoint(0, 0), image);
    return;
  }

  painter.drawImage(QPoint(0, 0), p->frontImage);
}

void RenderWidget::publishCompletedTiles() {
  if (!p->renderThread)
    return;

  for (const auto& tile : p->renderThread->engine->completedTiles()) {
    publishTile(tile);
  }
}

void RenderWidget::publishFullBackBuffer() {
  if (!p->backBuffer)
    return;

  publishTile(Recti(0, 0, p->backBuffer->width(), p->backBuffer->height()));
}

void RenderWidget::publishTile(const Recti& tile) {
  const int left = std::max(0, tile.left());
  const int top = std::max(0, tile.top());
  const int right = std::min(p->backBuffer->width(), tile.right());
  const int bottom = std::min(p->backBuffer->height(), tile.bottom());

  for (int x = left; x < right; ++x) {
    for (int y = top; y < bottom; ++y) {
      p->frontImage.setPixel(x, y, (*p->backBuffer)[y][x]);
    }
  }
}

void RenderWidget::markTilesInProgress(QImage& image) const {
  for (const auto& tile : p->renderThread->engine->activeTiles()) {
    const int left = std::max(0, tile.left());
    const int top = std::max(0, tile.top());
    const int right = std::min(image.width(), tile.right());
    const int bottom = std::min(image.height(), tile.bottom());
    for (int x = left; x != right; ++x) {
      for (int y = top; y != bottom; ++y) {
        image.setPixel(x, y, darken(image.pixel(x, y), 0.8));
      }
    }
  }
}

QRgb RenderWidget::darken(QRgb color, double factor) const {
  return qRgb(
    qRed(color) * factor,
    qGreen(color) * factor,
    qBlue(color) * factor
  );
}

void RenderWidget::renderThreadDone() {
  publishFullBackBuffer();
  if (p->timer != 0) {
    killTimer(p->timer);
    p->timer = 0;
  }
  update();
  emit finished();
}
