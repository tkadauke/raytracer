#pragma once

#include <QWidget>

namespace raytracer {
  class Raytracer;
}

class RenderWidget : public QWidget {
  Q_OBJECT
public:
  RenderWidget(QWidget* parent, std::shared_ptr<raytracer::Raytracer> raytracer);
  ~RenderWidget();
  
  virtual void paintEvent(QPaintEvent*);
  virtual void timerEvent(QTimerEvent *event);
  
  virtual void render();
  void setBufferSize(const QSize& size);

signals:
  void finished();
  
public slots:
  void stop();
  
protected:
  std::shared_ptr<raytracer::Raytracer> m_raytracer;

private slots:
  void renderThreadDone();

private:
  struct Private;
  std::unique_ptr<Private> p;
};
