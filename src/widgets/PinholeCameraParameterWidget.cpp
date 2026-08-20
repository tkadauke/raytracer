#include "widgets/CameraParameterWidgetFactory.h"
#include "widgets/PinholeCameraParameterWidget.h"
#include "ui_PinholeCameraParameterWidget.h"
#include "render/cameras/PinholeCamera.h"

using namespace render;

struct PinholeCameraParameterWidget::Private {
  Ui::PinholeCameraParameterWidget ui;
};

PinholeCameraParameterWidget::PinholeCameraParameterWidget(QWidget* parent)
    : CameraParameterWidget(parent),
      p(std::make_unique<Private>()) {
  p->ui.setupUi(this);
  connectValueChangedInputs(p->ui.distanceInput, p->ui.zoomInput);
}

PinholeCameraParameterWidget::~PinholeCameraParameterWidget() {
}

double PinholeCameraParameterWidget::distance() const {
  return p->ui.distanceInput->value();
}

double PinholeCameraParameterWidget::zoom() const {
  return p->ui.zoomInput->value();
}

void PinholeCameraParameterWidget::applyTo(std::shared_ptr<render::Camera> camera) {
  auto pinholeCamera = dynamic_cast<render::PinholeCamera*>(camera.get());
  if (pinholeCamera) {
    pinholeCamera->setDistance(distance());
    pinholeCamera->setZoom(zoom());
  }
}

static bool dummy =
  CameraParameterWidgetFactory::self().registerClass<PinholeCameraParameterWidget>("PinholeCamera");
