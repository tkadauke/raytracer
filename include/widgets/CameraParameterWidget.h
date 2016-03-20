#pragma once

#include <QWidget>

namespace raytracer {
  class Camera;
}

class CameraParameterWidget : public QWidget {
public:
  explicit CameraParameterWidget(QWidget* parent = nullptr);

  virtual void applyTo(std::shared_ptr<raytracer::Camera> camera) = 0;
};
