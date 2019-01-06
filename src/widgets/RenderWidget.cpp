#include "widgets/RenderWidget.h"
#include "raytracer/Raytracer.h"
#include "core/Buffer.h"

#include <QThread>
#include <QPainter>

using namespace std;
using namespace raytracer;

namespace {
  class RenderThread : public QThread {
  public:
    inline RenderThread(std::shared_ptr<Raytracer> rt, Buffer<unsigned int>& b)
      : raytracer(rt),
        buffer(b)
    {
    }

    inline virtual void run() {
      raytracer->render(buffer);
    }

    inline void cancel() {
      raytracer->cancel();
    }

    std::shared_ptr<Raytracer> raytracer;
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

RenderWidget::RenderWidget(QWidget* parent, std::shared_ptr<Raytracer> raytracer)
  : QWidget(parent),
    m_raytracer(raytracer),
    p(std::make_unique<Private>())
{
  setBufferSize(QSize(0, 0));
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
  m_raytracer->uncancel();
  p->renderThread = new RenderThread(m_raytracer, *p->buffer);
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
  for (const auto& rect : p->renderThread->raytracer->activeRects()) {
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

#include "RenderWidget.moc"
