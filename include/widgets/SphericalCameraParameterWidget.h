#pragma once

#include "widgets/CameraParameterWidget.h"

class SphericalCameraParameterWidget : public CameraParameterWidget {
  Q_OBJECT
  
public:
  SphericalCameraParameterWidget(QWidget* parent = nullptr);
  ~SphericalCameraParameterWidget();
  
  int horizontalFieldOfView() const;
  int verticalFieldOfView() const;

  virtual void applyTo(raytracer::Camera* camera);

signals:
  void changed();

private slots:
  void parameterChanged();

private:
  struct Private;
  Private* p;
};
