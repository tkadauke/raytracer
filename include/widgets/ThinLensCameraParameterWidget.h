#pragma once
#include <memory>

#include "widgets/CameraParameterWidget.h"

/**
  * Sidebar parameter editor for `render::ThinLensCamera` — exposes
  * `distance`, `zoom`, `apertureRadius`, and `focalDistance` as live
  * spin-box inputs in camera parameter UIs. Picked up automatically when the
  * user selects "ThinLensCamera" from a camera-type dropdown
  * (registered with `CameraParameterWidgetFactory`).
  *
  * Each spin-box change emits the inherited `changed()` signal so
  * `Display::cameraParameterChanged()` can call `applyTo()` and
  * re-render. Adjust `apertureRadius` to watch DOF blur grow; adjust
  * `focalDistance` to slide focus through the scene.
  */
class ThinLensCameraParameterWidget : public CameraParameterWidget {
  Q_OBJECT

public:
  explicit ThinLensCameraParameterWidget(QWidget* parent = nullptr);
  ~ThinLensCameraParameterWidget();

  double distance() const;
  double zoom() const;
  double apertureRadius() const;
  double focalDistance() const;

  /** Pushes the four spin-box values onto a (presumed) ThinLensCamera. */
  virtual void applyTo(std::shared_ptr<render::Camera> camera);

signals:
  void changed();

private slots:
  void parameterChanged();

private:
  struct Private;
  std::unique_ptr<Private> p;
};
