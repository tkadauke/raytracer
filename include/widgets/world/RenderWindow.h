#pragma once

#include <QWidget>

class Scene;

class RenderWidget;
class RenderSettingsWidget;

namespace raytracer {
  class Raytracer;
}

class RenderWindow : public QWidget {
  Q_OBJECT;

public:
  RenderWindow(QWidget* parent = nullptr);
  void setScene(::Scene* scene);

  virtual QSize sizeHint() const;
  virtual void timerEvent(QTimerEvent*);
  
  bool isBusy() const;

public slots:
  void render();
  void stop();

private slots:
  void finished();

private:
  struct Private;
  std::unique_ptr<Private> p;
};
