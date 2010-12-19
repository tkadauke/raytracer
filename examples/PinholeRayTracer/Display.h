#ifndef DISPLAY_H
#define DISPLAY_H

#include "widgets/QtDisplay.h"

class Scene;
class PinholeCamera;
class PinholeCameraParameterWidget;

class Display : public QtDisplay {
  Q_OBJECT

public:
  Display(Scene* scene);
  ~Display();

private slots:
  void parameterChanged();

private:
  PinholeCamera* m_camera;
  PinholeCameraParameterWidget* m_parameters;
};

#endif
