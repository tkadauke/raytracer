#include "examples/PinholeRayTracer/Display.h"
#include "Raytracer.h"
#include "PinholeCamera.h"
#include "widgets/PinholeCameraParameterWidget.h"

Display::Display(Scene* scene)
  : QtDisplay(new Raytracer(scene)), m_camera(new PinholeCamera)
{
  m_parameters = new PinholeCameraParameterWidget(this);
  m_raytracer->setCamera(m_camera);
  connect(m_parameters, SIGNAL(changed()), this, SLOT(parameterChanged()));
}

Display::~Display() {
  delete m_parameters;
}

void Display::parameterChanged() {
  m_camera->setDistance(m_parameters->distance());
  render();
}

#include "Display.moc"
