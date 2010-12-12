#include "examples/SphericalRayTracer/Display.h"
#include "Raytracer.h"
#include "SphericalCamera.h"
#include "widgets/SphericalCameraParameterWidget.h"

Display::Display(Scene* scene)
  : QtDisplay(new Raytracer(scene)), m_camera(new SphericalCamera(180, 90))
{
  m_parameters = new SphericalCameraParameterWidget(this);
  m_raytracer->setCamera(m_camera);
  connect(m_parameters, SIGNAL(changed()), this, SLOT(parameterChanged()));
}

Display::~Display() {
  delete m_parameters;
}

void Display::parameterChanged() {
  m_camera->setFieldOfView(m_parameters->horizontalFieldOfView(), m_parameters->verticalFieldOfView());
  render();
}

#include "Display.moc"
