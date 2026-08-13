#include "widgets/CameraTypeWidget.h"
#include "ui_CameraTypeWidget.h"
#include "render/cameras/CameraFactory.h"

using namespace std;
using namespace render;

struct CameraTypeWidget::Private {
  Ui::CameraTypeWidget ui;
};

CameraTypeWidget::CameraTypeWidget(QWidget* parent)
    : TypeSelectorWidget(parent),
      p(std::make_unique<Private>()) {
  p->ui.setupUi(this);
  populateComboBox(p->ui.cameraTypeComboBox, render::CameraFactory::self().identifiers());
}

CameraTypeWidget::~CameraTypeWidget() {
}

string CameraTypeWidget::type() const {
  return p->ui.cameraTypeComboBox->currentText().toStdString();
}
