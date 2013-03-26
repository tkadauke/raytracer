#include "widgets/CameraParameterWidgetFactory.h"
#include "widgets/FishEyeCameraParameterWidget.h"
#include "FishEyeCameraParameterWidget.uic"
#include "raytracer/cameras/FishEyeCamera.h"

using namespace raytracer;

struct FishEyeCameraParameterWidget::Private {
  Ui::FishEyeCameraParameterWidget ui;
};

FishEyeCameraParameterWidget::FishEyeCameraParameterWidget(QWidget* parent)
  : CameraParameterWidget(parent), p(new Private)
{
  p->ui.setupUi(this);
  connect(p->ui.m_fieldOfViewSlider, SIGNAL(valueChanged(int)), this, SLOT(parameterChanged()));
}

FishEyeCameraParameterWidget::~FishEyeCameraParameterWidget() {
  delete p;
}

void FishEyeCameraParameterWidget::parameterChanged() {
  emit changed();
}

int FishEyeCameraParameterWidget::fieldOfView() const {
  return p->ui.m_fieldOfViewSlider->value();
}

void FishEyeCameraParameterWidget::applyTo(Camera* camera) {
  FishEyeCamera* fishEyeCamera = dynamic_cast<FishEyeCamera*>(camera);
  if (fishEyeCamera) {
    fishEyeCamera->setFieldOfView(fieldOfView());
  }
}

static bool dummy = CameraParameterWidgetFactory::self().registerClass<FishEyeCameraParameterWidget>("FishEyeCamera");

#include "FishEyeCameraParameterWidget.moc"
