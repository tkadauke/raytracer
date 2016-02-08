#pragma once

#include "widgets/CameraParameterWidget.h"

class FishEyeCameraParameterWidget : public CameraParameterWidget {
  Q_OBJECT
  
public:
  FishEyeCameraParameterWidget(QWidget* parent = nullptr);
  ~FishEyeCameraParameterWidget();
  
  int fieldOfView() const;
  
  virtual void applyTo(raytracer::Camera* camera);

signals:
  void changed();

private slots:
  void parameterChanged();

private:
  struct Private;
  Private* p;
};
