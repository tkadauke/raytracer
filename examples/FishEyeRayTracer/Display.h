#ifndef DISPLAY_H
#define DISPLAY_H

#include "QtDisplay.h"

class Scene;
class FishEyeCamera;
class FishEyeCameraParameterWidget;

class Display : public QtDisplay {
  Q_OBJECT

public:
  Display(Scene* scene);
  ~Display();

private slots:
  void parameterChanged();

private:
  FishEyeCamera* m_camera;
  FishEyeCameraParameterWidget* m_parameters;
};

#endif
