#include "widgets/CameraParameterWidgetFactory.h"
#include "widgets/PinholeCameraParameterWidget.h"
#include "PinholeCameraParameterWidget.uic"
#include "raytracer/cameras/PinholeCamera.h"

using namespace raytracer;

struct PinholeCameraParameterWidget::Private {
  Ui::PinholeCameraParameterWidget ui;
};

PinholeCameraParameterWidget::PinholeCameraParameterWidget(QWidget* parent)
  : CameraParameterWidget(parent), p(new Private)
{
  p->ui.setupUi(this);
  connect(p->ui.m_distanceInput, SIGNAL(valueChanged(double)), this, SLOT(parameterChanged()));
  connect(p->ui.m_zoomInput, SIGNAL(valueChanged(double)), this, SLOT(parameterChanged()));
}

PinholeCameraParameterWidget::~PinholeCameraParameterWidget() {
  delete p;
}

void PinholeCameraParameterWidget::parameterChanged() {
  emit changed();
}

double PinholeCameraParameterWidget::distance() const {
  return p->ui.m_distanceInput->value();
}

double PinholeCameraParameterWidget::zoom() const {
  return p->ui.m_zoomInput->value();
}

void PinholeCameraParameterWidget::applyTo(Camera* camera) {
  PinholeCamera* pinholeCamera = dynamic_cast<PinholeCamera*>(camera);
  if (pinholeCamera) {
    pinholeCamera->setDistance(distance());
    pinholeCamera->setZoom(zoom());
  }
}

static bool dummy = CameraParameterWidgetFactory::self().registerClass<PinholeCameraParameterWidget>("PinholeCamera");

#include "PinholeCameraParameterWidget.moc"
