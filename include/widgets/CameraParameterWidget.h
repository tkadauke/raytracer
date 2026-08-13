#pragma once
#include <memory>

#include <QWidget>

namespace render {
  class Camera;
}

class CameraParameterWidget : public QWidget {
  Q_OBJECT

public:
  explicit CameraParameterWidget(QWidget* parent = nullptr);

  virtual void applyTo(std::shared_ptr<render::Camera> camera) = 0;

signals:
  void changed();

protected slots:
  void parameterChanged();
};
