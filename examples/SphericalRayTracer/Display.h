#ifndef DISPLAY_H
#define DISPLAY_H

#include "QtDisplay.h"

class Scene;
class SphericalCamera;
class SphericalCameraParameterWidget;

class Display : public QtDisplay {
  Q_OBJECT

public:
  Display(Scene* scene);
  ~Display();

private slots:
  void parameterChanged();

private:
  SphericalCamera* m_camera;
  SphericalCameraParameterWidget* m_parameters;
};

#endif
