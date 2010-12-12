#include "widgets/PinholeCameraParameterWidget.h"
#include "PinholeCameraParameterWidget.uic"

struct PinholeCameraParameterWidget::Private {
  Ui::PinholeCameraParameterWidget ui;
};

PinholeCameraParameterWidget::PinholeCameraParameterWidget(QWidget* parent)
  : QWidget(parent, Qt::Drawer), p(new Private)
{
  p->ui.setupUi(this);
  connect(p->ui.m_distanceInput, SIGNAL(valueChanged(double)), this, SLOT(parameterChanged()));
}

PinholeCameraParameterWidget::~PinholeCameraParameterWidget() {
  delete p;
}

void PinholeCameraParameterWidget::parameterChanged() {
  emit changed();
}

double PinholeCameraParameterWidget::distance() const {
  return p->ui.m_distanceInput->value();
}

#include "PinholeCameraParameterWidget.moc"
