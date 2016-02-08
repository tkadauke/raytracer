#pragma once

#include <QWidget>

namespace raytracer {
  class Camera;
}

class CameraParameterWidget : public QWidget {
public:
  CameraParameterWidget(QWidget* parent = nullptr);

  virtual void applyTo(raytracer::Camera* camera) = 0;
};
