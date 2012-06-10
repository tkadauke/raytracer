#ifndef DISPLAY_H
#define DISPLAY_H

#include "widgets/QtDisplay.h"

class QVBoxLayout;
class Scene;
class SceneWidget;
class ViewPlaneTypeWidget;
class CameraTypeWidget;
class CameraParameterWidget;
class Camera;

class Display : public QtDisplay {
  Q_OBJECT

public:
  Display();
  ~Display();

private slots:
  void sceneChanged();
  void viewPlaneTypeChanged();
  void cameraTypeChanged();
  void cameraParameterChanged();

private:
  QVBoxLayout* m_verticalLayout;
  Camera* m_camera;
  QWidget* m_sidebar;
  SceneWidget* m_scene;
  ViewPlaneTypeWidget* m_viewPlaneType;
  CameraTypeWidget* m_cameraType;
  CameraParameterWidget* m_cameraParameter;
};

#endif
