#pragma once

#include "widgets/CameraParameterWidget.h"

class FishEyeCameraParameterWidget : public CameraParameterWidget {
  Q_OBJECT;
  
public:
  explicit FishEyeCameraParameterWidget(QWidget* parent = nullptr);
  ~FishEyeCameraParameterWidget();
  
  int fieldOfView() const;
  
  virtual void applyTo(std::shared_ptr<raytracer::Camera> camera);

signals:
  void changed();

private slots:
  void parameterChanged();

private:
  struct Private;
  std::unique_ptr<Private> p;
};
