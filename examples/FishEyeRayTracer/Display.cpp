#include "examples/FishEyeRayTracer/Display.h"
#include "Raytracer.h"
#include "FishEyeCamera.h"
#include "widgets/FishEyeCameraParameterWidget.h"

Display::Display(Scene* scene)
  : QtDisplay(new Raytracer(scene)), m_camera(new FishEyeCamera(90))
{
  m_parameters = new FishEyeCameraParameterWidget(this);
  m_raytracer->setCamera(m_camera);
  connect(m_parameters, SIGNAL(changed()), this, SLOT(parameterChanged()));
}

Display::~Display() {
  delete m_parameters;
}

void Display::parameterChanged() {
  m_camera->setFieldOfView(m_parameters->fieldOfView());
  render();
}

#include "Display.moc"
