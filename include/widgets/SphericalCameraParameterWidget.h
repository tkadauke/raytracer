#pragma once

#include "widgets/CameraParameterWidget.h"

class SphericalCameraParameterWidget : public CameraParameterWidget {
  Q_OBJECT;
  
public:
  explicit SphericalCameraParameterWidget(QWidget* parent = nullptr);
  ~SphericalCameraParameterWidget();
  
  int horizontalFieldOfView() const;
  int verticalFieldOfView() const;

  virtual void applyTo(std::shared_ptr<raytracer::Camera> camera);

signals:
  void changed();

private slots:
  void parameterChanged();

private:
  struct Private;
  std::unique_ptr<Private> p;
};
