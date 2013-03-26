#ifndef PINHOLE_CAMERA_PARAMETER_WIDGET_H
#define PINHOLE_CAMERA_PARAMETER_WIDGET_H

#include "widgets/CameraParameterWidget.h"

class PinholeCameraParameterWidget : public CameraParameterWidget {
  Q_OBJECT
  
public:
  PinholeCameraParameterWidget(QWidget* parent = 0);
  ~PinholeCameraParameterWidget();
  
  double distance() const;
  double zoom() const;

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
