#ifndef SPHERICAL_CAMERA_PARAMETER_WIDGET_H
#define SPHERICAL_CAMERA_PARAMETER_WIDGET_H

#include "widgets/CameraParameterWidget.h"

class SphericalCameraParameterWidget : public CameraParameterWidget {
  Q_OBJECT
  
public:
  SphericalCameraParameterWidget(QWidget* parent = 0);
  ~SphericalCameraParameterWidget();
  
  int horizontalFieldOfView() const;
  int verticalFieldOfView() const;

  virtual void applyTo(Camera* camera);

signals:
  void changed();

private slots:
  void parameterChanged();

private:
  struct Private;
  Private* p;
};

#endif
