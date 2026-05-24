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
  // Update the value labels when the sliders move.  Use lambdas rather than
  // &QLabel::setNum to avoid the int/double overload ambiguity in Qt 6.
  connect(p->ui.horizontalFieldOfViewSlider, &QSlider::valueChanged, this,
          [this](int v) { p->ui.horizontalFieldOfViewLabel->setNum(v); });
  connect(p->ui.verticalFieldOfViewSlider, &QSlider::valueChanged, this,
          [this](int v) { p->ui.verticalFieldOfViewLabel->setNum(v); });
  connect(p->ui.horizontalFieldOfViewSlider, SIGNAL(valueChanged(int)), this,
          SLOT(parameterChanged()));
  connect(p->ui.verticalFieldOfViewSlider, SIGNAL(valueChanged(int)), this,
          SLOT(parameterChanged()));
}

SphericalCameraParameterWidget::~SphericalCameraParameterWidget() {
}

void SphericalCameraParameterWidget::parameterChanged() {
  emit changed();
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
