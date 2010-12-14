#include "examples/ViewPlaneRayTracer/Display.h"
#include "Raytracer.h"
#include "PinholeCamera.h"
#include "widgets/ViewPlaneTypeWidget.h"
#include "ViewPlaneFactory.h"

Display::Display(Scene* scene)
  : QtDisplay(new Raytracer(scene)), m_camera(new PinholeCamera)
{
  m_parameters = new ViewPlaneTypeWidget(this);
  m_raytracer->setCamera(m_camera);
  m_camera->setViewPlane(ViewPlaneFactory::self().create(m_parameters->type()));
  connect(m_parameters, SIGNAL(changed()), this, SLOT(typeChanged()));
}

Display::~Display() {
  delete m_parameters;
}

void Display::typeChanged() {
  m_camera->setViewPlane(ViewPlaneFactory::self().create(m_parameters->type()));
  render();
}

#include "Display.moc"
