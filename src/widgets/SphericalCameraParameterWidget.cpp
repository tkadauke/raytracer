#include "widgets/SphericalCameraParameterWidget.h"
#include "SphericalCameraParameterWidget.uic"

struct SphericalCameraParameterWidget::Private {
  Ui::SphericalCameraParameterWidget ui;
};

SphericalCameraParameterWidget::SphericalCameraParameterWidget(QWidget* parent)
  : QWidget(parent, Qt::Drawer), p(new Private)
{
  p->ui.setupUi(this);
  connect(p->ui.m_horizontalFieldOfViewSlider, SIGNAL(valueChanged(int)), this, SLOT(parameterChanged()));
  connect(p->ui.m_verticalFieldOfViewSlider, SIGNAL(valueChanged(int)), this, SLOT(parameterChanged()));
}

SphericalCameraParameterWidget::~SphericalCameraParameterWidget() {
  delete p;
}

void SphericalCameraParameterWidget::parameterChanged() {
  emit changed();
}

int SphericalCameraParameterWidget::horizontalFieldOfView() const {
  return p->ui.m_horizontalFieldOfViewSlider->value();
}

int SphericalCameraParameterWidget::verticalFieldOfView() const {
  return p->ui.m_verticalFieldOfViewSlider->value();
}

#include "SphericalCameraParameterWidget.moc"
