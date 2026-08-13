#pragma once
#include <memory>

#include "widgets/CameraParameterWidget.h"

class PinholeCameraParameterWidget : public CameraParameterWidget {
  Q_OBJECT

public:
  explicit PinholeCameraParameterWidget(QWidget* parent = nullptr);
  ~PinholeCameraParameterWidget();

  double distance() const;
  double zoom() const;

  virtual void applyTo(std::shared_ptr<render::Camera> camera);

private:
  struct Private;
  std::unique_ptr<Private> p;
};
