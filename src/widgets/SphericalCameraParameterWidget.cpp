#include "widgets/CameraParameterWidgetFactory.h"
#include "widgets/SphericalCameraParameterWidget.h"
#include "SphericalCameraParameterWidget.uic"
#include "raytracer/cameras/SphericalCamera.h"

using namespace raytracer;

struct SphericalCameraParameterWidget::Private {
  Ui::SphericalCameraParameterWidget ui;
};

SphericalCameraParameterWidget::SphericalCameraParameterWidget(QWidget* parent)
  : CameraParameterWidget(parent), p(new Private)
{
  p->ui.setupUi(this);
  connect(p->ui.m_horizontalFieldOfViewSlider, SIGNAL(valueChanged(int)), this, SLOT(parameterChanged()));
  connect(p->ui.m_verticalFieldOfViewSlider, SIGNAL(valueChanged(int)), this, SLOT(parameterChanged()));
}

SphericalCameraParameterWidget::~SphericalCameraParameterWidget() {
  delete p;
}

void SphericalCameraParameterWidget::parameterChanged() {
  emit changed();
}

int SphericalCameraParameterWidget::horizontalFieldOfView() const {
  return p->ui.m_horizontalFieldOfViewSlider->value();
}

int SphericalCameraParameterWidget::verticalFieldOfView() const {
  return p->ui.m_verticalFieldOfViewSlider->value();
}

void SphericalCameraParameterWidget::applyTo(Camera* camera) {
  SphericalCamera* sphericalCamera = dynamic_cast<SphericalCamera*>(camera);
  if (sphericalCamera) {
    sphericalCamera->setHorizontalFieldOfView(horizontalFieldOfView());
    sphericalCamera->setVerticalFieldOfView(verticalFieldOfView());
  }
}

static bool dummy = CameraParameterWidgetFactory::self().registerClass<SphericalCameraParameterWidget>("SphericalCamera");

#include "SphericalCameraParameterWidget.moc"
