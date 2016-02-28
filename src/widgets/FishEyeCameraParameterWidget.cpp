#include "widgets/CameraParameterWidgetFactory.h"
#include "widgets/FishEyeCameraParameterWidget.h"
#include "FishEyeCameraParameterWidget.uic"
#include "raytracer/cameras/FishEyeCamera.h"

using namespace raytracer;

struct FishEyeCameraParameterWidget::Private {
  Ui::FishEyeCameraParameterWidget ui;
};

FishEyeCameraParameterWidget::FishEyeCameraParameterWidget(QWidget* parent)
  : CameraParameterWidget(parent),
    p(std::make_unique<Private>())
{
  p->ui.setupUi(this);
  connect(p->ui.fieldOfViewSlider, SIGNAL(valueChanged(int)), this, SLOT(parameterChanged()));
}

FishEyeCameraParameterWidget::~FishEyeCameraParameterWidget() {
}

void FishEyeCameraParameterWidget::parameterChanged() {
  emit changed();
}

int FishEyeCameraParameterWidget::fieldOfView() const {
  return p->ui.fieldOfViewSlider->value();
}

void FishEyeCameraParameterWidget::applyTo(std::shared_ptr<Camera> camera) {
  auto fishEyeCamera = dynamic_cast<FishEyeCamera*>(camera.get());
  if (fishEyeCamera) {
    fishEyeCamera->setFieldOfView(Angled::fromDegrees(fieldOfView()));
  }
}

static bool dummy = CameraParameterWidgetFactory::self().registerClass<FishEyeCameraParameterWidget>("FishEyeCamera");

#include "FishEyeCameraParameterWidget.moc"
