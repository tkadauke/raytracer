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
  // Update the value label when the slider moves.  Use a lambda rather than
  // &QLabel::setNum to avoid the int/double overload ambiguity in Qt 6.
  connect(p->ui.fieldOfViewSlider, &QSlider::valueChanged, this,
          [this](int v) { p->ui.fieldOfViewLabel->setNum(v); });
  connect(p->ui.fieldOfViewSlider, SIGNAL(valueChanged(int)), this, SLOT(parameterChanged()));
  connect(p->ui.fieldOfViewSlider, &QSlider::valueChanged, p->ui.fieldOfViewLabel,
          qOverload<int>(&QLabel::setNum));
}

FishEyeCameraParameterWidget::~FishEyeCameraParameterWidget() {
}

void FishEyeCameraParameterWidget::parameterChanged() {
  emit changed();
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
