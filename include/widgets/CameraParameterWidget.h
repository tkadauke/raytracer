#pragma once
#include <memory>

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QSlider;

namespace render {
  class Camera;
}

class CameraParameterWidget : public QWidget {
  Q_OBJECT

public:
  explicit CameraParameterWidget(QWidget* parent = nullptr);

  virtual void applyTo(std::shared_ptr<render::Camera> camera) = 0;

signals:
  void changed();

protected slots:
  void parameterChanged();

protected:
  /**
    * Wires a degree-valued slider to update its value @p label and to
    * notify `changed()` on every move. Shared by the camera parameter
    * widgets whose UI exposes a field-of-view slider plus a live-updating
    * numeric label (`FishEyeCameraParameterWidget`,
    * `SphericalCameraParameterWidget`).
    */
  void connectDegreeSlider(QSlider* slider, QLabel* label);

  /**
    * Wires each of @p inputs' `valueChanged(double)` signal to
    * `parameterChanged()`. Shared by the camera parameter widgets whose UI
    * is a flat list of double spin boxes (`PinholeCameraParameterWidget`,
    * `ThinLensCameraParameterWidget`).
    */
  template <typename... Inputs>
  void connectValueChangedInputs(Inputs*... inputs) {
    (connect(inputs, SIGNAL(valueChanged(double)), this, SLOT(parameterChanged())), ...);
  }
};
