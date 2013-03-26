#ifndef FISH_EYE_CAMERA_PARAMETER_WIDGET_H
#define FISH_EYE_CAMERA_PARAMETER_WIDGET_H

#include "widgets/CameraParameterWidget.h"

class FishEyeCameraParameterWidget : public CameraParameterWidget {
  Q_OBJECT
  
public:
  FishEyeCameraParameterWidget(QWidget* parent = 0);
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

#endif
