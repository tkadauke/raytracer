#pragma once

#include "widgets/CameraParameterWidget.h"

class PinholeCameraParameterWidget : public CameraParameterWidget {
  Q_OBJECT;
  
public:
  explicit PinholeCameraParameterWidget(QWidget* parent = nullptr);
  ~PinholeCameraParameterWidget();
  
  double distance() const;
  double zoom() const;

  virtual void applyTo(std::shared_ptr<raytracer::Camera> camera);

signals:
  void changed();

private slots:
  void parameterChanged();

private:
  struct Private;
  std::unique_ptr<Private> p;
};
