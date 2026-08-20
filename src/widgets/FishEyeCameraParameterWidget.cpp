#include "widgets/CameraParameterWidgetFactory.h"
#include "widgets/FishEyeCameraParameterWidget.h"
#include "ui_FishEyeCameraParameterWidget.h"
#include "render/cameras/FishEyeCamera.h"

#include <QLabel>
#include <QSlider>

using namespace render;

struct FishEyeCameraParameterWidget::Private {
  Ui::FishEyeCameraParameterWidget ui;
};

FishEyeCameraParameterWidget::FishEyeCameraParameterWidget(QWidget* parent)
    : CameraParameterWidget(parent),
      p(std::make_unique<Private>()) {
  p->ui.setupUi(this);
  connectDegreeSlider(p->ui.fieldOfViewSlider, p->ui.fieldOfViewLabel);
}

FishEyeCameraParameterWidget::~FishEyeCameraParameterWidget() {
}

int FishEyeCameraParameterWidget::fieldOfView() const {
  return p->ui.fieldOfViewSlider->value();
}

void FishEyeCameraParameterWidget::applyTo(std::shared_ptr<render::Camera> camera) {
  auto fishEyeCamera = dynamic_cast<render::FishEyeCamera*>(camera.get());
  if (fishEyeCamera) {
    fishEyeCamera->setFieldOfView(Angled::fromDegrees(fieldOfView()));
  }
}

static bool dummy =
  CameraParameterWidgetFactory::self().registerClass<FishEyeCameraParameterWidget>("FishEyeCamera");
