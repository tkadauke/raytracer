#ifndef CAMERA_PARAMETER_WIDGET_H
#define CAMERA_PARAMETER_WIDGET_H

#include <QWidget>

namespace raytracer {
  class Camera;
}

class CameraParameterWidget : public QWidget {
public:
  CameraParameterWidget(QWidget* parent = 0);

  virtual void applyTo(raytracer::Camera* camera) = 0;
};

#endif
