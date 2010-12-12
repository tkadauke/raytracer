#include "widgets/FishEyeCameraParameterWidget.h"
#include "FishEyeCameraParameterWidget.uic"

struct FishEyeCameraParameterWidget::Private {
  Ui::FishEyeCameraParameterWidget ui;
};

FishEyeCameraParameterWidget::FishEyeCameraParameterWidget(QWidget* parent)
  : QWidget(parent, Qt::Drawer), p(new Private)
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

#include "FishEyeCameraParameterWidget.moc"
