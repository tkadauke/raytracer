#include "widgets/CameraParameterWidgetFactory.h"
#include "widgets/ThinLensCameraParameterWidget.h"
#include "ui_ThinLensCameraParameterWidget.h"
#include "render/cameras/ThinLensCamera.h"

using namespace render;

struct ThinLensCameraParameterWidget::Private {
  Ui::ThinLensCameraParameterWidget ui;
};

ThinLensCameraParameterWidget::ThinLensCameraParameterWidget(QWidget* parent)
    : CameraParameterWidget(parent),
      p(std::make_unique<Private>()) {
  p->ui.setupUi(this);
  connectValueChangedInputs(p->ui.distanceInput, p->ui.zoomInput, p->ui.apertureRadiusInput,
                            p->ui.focalDistanceInput);
}

ThinLensCameraParameterWidget::~ThinLensCameraParameterWidget() {
}

double ThinLensCameraParameterWidget::distance() const {
  return p->ui.distanceInput->value();
}

double ThinLensCameraParameterWidget::zoom() const {
  return p->ui.zoomInput->value();
}

double ThinLensCameraParameterWidget::apertureRadius() const {
  return p->ui.apertureRadiusInput->value();
}

double ThinLensCameraParameterWidget::focalDistance() const {
  return p->ui.focalDistanceInput->value();
}

void ThinLensCameraParameterWidget::applyTo(std::shared_ptr<render::Camera> camera) {
  // dynamic_cast guards against a registry / dropdown mismatch (the
  // factory hands us this widget when the user picks "ThinLensCamera",
  // so the camera *should* be a ThinLensCamera, but pin the contract).
  auto thinLens = dynamic_cast<render::ThinLensCamera*>(camera.get());
  if (thinLens) {
    thinLens->setDistance(distance());
    thinLens->setZoom(zoom());
    thinLens->setApertureRadius(apertureRadius());
    thinLens->setFocalDistance(focalDistance());
  }
}

static bool dummy =
  CameraParameterWidgetFactory::self().registerClass<ThinLensCameraParameterWidget>(
    "ThinLensCamera");
