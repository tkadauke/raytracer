#include "widgets/CameraParameterWidgetFactory.h"
#include "widgets/SphericalCameraParameterWidget.h"
#include "ui_SphericalCameraParameterWidget.h"
#include "render/cameras/SphericalCamera.h"

#include <QLabel>
#include <QSlider>

using namespace render;

struct SphericalCameraParameterWidget::Private {
  Ui::SphericalCameraParameterWidget ui;
};

SphericalCameraParameterWidget::SphericalCameraParameterWidget(QWidget* parent)
    : CameraParameterWidget(parent),
      p(std::make_unique<Private>()) {
  p->ui.setupUi(this);
  connectDegreeSlider(p->ui.horizontalFieldOfViewSlider, p->ui.horizontalFieldOfViewLabel);
  connectDegreeSlider(p->ui.verticalFieldOfViewSlider, p->ui.verticalFieldOfViewLabel);
}

SphericalCameraParameterWidget::~SphericalCameraParameterWidget() {
}

int SphericalCameraParameterWidget::horizontalFieldOfView() const {
  return p->ui.horizontalFieldOfViewSlider->value();
}

int SphericalCameraParameterWidget::verticalFieldOfView() const {
  return p->ui.verticalFieldOfViewSlider->value();
}

void SphericalCameraParameterWidget::applyTo(std::shared_ptr<render::Camera> camera) {
  auto sphericalCamera = dynamic_cast<render::SphericalCamera*>(camera.get());
  if (sphericalCamera) {
    sphericalCamera->setHorizontalFieldOfView(Angled::fromDegrees(horizontalFieldOfView()));
    sphericalCamera->setVerticalFieldOfView(Angled::fromDegrees(verticalFieldOfView()));
  }
}

static bool dummy =
  CameraParameterWidgetFactory::self().registerClass<SphericalCameraParameterWidget>(
    "SphericalCamera");
