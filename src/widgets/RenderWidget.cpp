#include "widgets/RenderWidget.h"
#include "render/RenderEngine.h"
#include "core/Buffer.h"

#include <QThread>
#include <QPainter>

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
      buffer(nullptr),
      timer(0),
      showProgressIndicators(false)
  {
  }

  RenderThread* renderThread;

  Buffer<unsigned int>* buffer;
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

std::shared_ptr<render::RenderEngine> RenderWidget::engine() const {
  return m_engine;
}

RenderWidget::~RenderWidget() {
}

void RenderWidget::stop() {
  if (p->renderThread && p->renderThread->isRunning()) {
    p->renderThread->cancel();
    p->renderThread->wait();
  }

  if (p->renderThread) {
    delete p->renderThread;
    p->renderThread = nullptr;
  }
  update();
}

void RenderWidget::render() {
  stop();
  m_engine->uncancel();
  p->renderThread = new RenderThread(m_engine, *p->buffer);
  p->renderThread->start();
  connect(p->renderThread, SIGNAL(finished()), this, SLOT(renderThreadDone()));

  auto interval = p->buffer->width();

  p->timer = startTimer(interval / 10);
}

void RenderWidget::setBufferSize(const QSize& size) {
  if (p->buffer)
    delete p->buffer;
  p->buffer = new Buffer<unsigned int>(size.width(), size.height());
  p->buffer->clear();
}

void RenderWidget::setShowProgressIndicators(bool show) {
  p->showProgressIndicators = show;
}

void RenderWidget::timerEvent(QTimerEvent*) {
  update();
}

void RenderWidget::paintEvent(QPaintEvent*) {
  if (p->renderThread && !p->renderThread->isRunning())
    stop();

  QPainter painter(this);
  QImage image(p->buffer->width(), p->buffer->height(), QImage::Format_RGB32);

  for (int i = 0; i != p->buffer->width(); ++i) {
    for (int j = 0; j != p->buffer->height(); ++j) {
      image.setPixel(i, j, (*p->buffer)[j][i]);
    }
  }

  if (p->renderThread && p->renderThread->isRunning() && p->showProgressIndicators) {
    markRectsInProgress(image);
  }

  painter.drawImage(QPoint(0, 0), image);
}

void RenderWidget::markRectsInProgress(QImage& image) const {
  for (const auto& rect : p->renderThread->engine->activeRects()) {
    for (int x = rect.left(); x != rect.right(); ++x) {
      for (int y = rect.top(); y != rect.bottom(); ++y) {
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
  update();
  emit finished();
}

