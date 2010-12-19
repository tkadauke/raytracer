#ifndef DISPLAY_H
#define DISPLAY_H

#include "widgets/QtDisplay.h"

class Scene;
class ViewPlaneTypeWidget;
class Camera;

class Display : public QtDisplay {
  Q_OBJECT

public:
  Display(Scene* scene);
  ~Display();

private slots:
  void typeChanged();

private:
  Camera* m_camera;
  ViewPlaneTypeWidget* m_parameters;
};

#endif
