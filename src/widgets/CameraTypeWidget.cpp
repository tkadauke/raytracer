#include "widgets/CameraTypeWidget.h"
#include "CameraTypeWidget.uic"
#include "raytracer/cameras/CameraFactory.h"

#include <list>

using namespace std;
using namespace raytracer;

struct CameraTypeWidget::Private {
  Ui::CameraTypeWidget ui;
};

CameraTypeWidget::CameraTypeWidget(QWidget* parent)
  : QWidget(parent),
    p(std::make_unique<Private>())
{
  p->ui.setupUi(this);
  list<string> types = CameraFactory::self().identifiers();
  for (const auto& type : types) {
    p->ui.cameraTypeComboBox->addItem(QString::fromStdString(type));
  }
  connect(p->ui.cameraTypeComboBox, SIGNAL(activated(int)), this, SLOT(typeChanged()));
}

CameraTypeWidget::~CameraTypeWidget() {
}

string CameraTypeWidget::type() const {
  return p->ui.cameraTypeComboBox->currentText().toStdString();
}

void CameraTypeWidget::typeChanged() {
  emit changed();
}

#include "CameraTypeWidget.moc"
