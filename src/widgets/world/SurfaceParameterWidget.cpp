#include "widgets/world/SurfaceParameterWidget.h"
#include "SurfaceParameterWidget.uic"
#include "world/objects/Surface.h"

struct SurfaceParameterWidget::Private {
  Ui::SurfaceParameterWidget ui;
};

SurfaceParameterWidget::SurfaceParameterWidget(QWidget* parent)
  : QWidget(parent), p(new Private)
{
  p->ui.setupUi(this);
  connect(p->ui.m_visibleCheckBox, SIGNAL(stateChanged(int)), this, SLOT(parameterChanged()));
  connect(p->ui.m_positionXEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  connect(p->ui.m_positionYEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  connect(p->ui.m_positionZEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  
  connect(p->ui.m_scaleXEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  connect(p->ui.m_scaleYEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  connect(p->ui.m_scaleZEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  
  connect(p->ui.m_orientationWEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  connect(p->ui.m_orientationXEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  connect(p->ui.m_orientationYEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  connect(p->ui.m_orientationZEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
}

SurfaceParameterWidget::~SurfaceParameterWidget() {
  delete p;
}

void SurfaceParameterWidget::parameterChanged() {
  emit changed();
}

bool SurfaceParameterWidget::visible() const {
  return p->ui.m_visibleCheckBox->checkState() == Qt::Checked;
}

void SurfaceParameterWidget::setVisible(bool visible) {
  p->ui.m_visibleCheckBox->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
}

Vector3d SurfaceParameterWidget::position() const {
  return Vector3d(
    p->ui.m_positionXEdit->text().toDouble(),
    p->ui.m_positionYEdit->text().toDouble(),
    p->ui.m_positionZEdit->text().toDouble()
  );
}

void SurfaceParameterWidget::setPosition(const Vector3d& position) {
  p->ui.m_positionXEdit->setText(QString::number(position.x()));
  p->ui.m_positionYEdit->setText(QString::number(position.y()));
  p->ui.m_positionZEdit->setText(QString::number(position.z()));
}

Vector3d SurfaceParameterWidget::scale() const {
  return Vector3d(
    p->ui.m_scaleXEdit->text().toDouble(),
    p->ui.m_scaleYEdit->text().toDouble(),
    p->ui.m_scaleZEdit->text().toDouble()
  );
}

void SurfaceParameterWidget::setScale(const Vector3d& scale) {
  
}

Quaterniond SurfaceParameterWidget::orientation() const {
  return Quaterniond(
    p->ui.m_orientationWEdit->text().toDouble(),
    p->ui.m_orientationXEdit->text().toDouble(),
    p->ui.m_orientationYEdit->text().toDouble(),
    p->ui.m_orientationZEdit->text().toDouble()
  );
}

void SurfaceParameterWidget::setOrientation(const Vector3d& orientation) {
  
}

void SurfaceParameterWidget::applyTo(Element* element) {
  Surface* surface = dynamic_cast<Surface*>(element);
  if (surface) {
    surface->setVisible(visible());
    surface->setPosition(position());
    surface->setScale(scale());
    surface->setOrientation(orientation());
  }
}

void SurfaceParameterWidget::getFrom(Element* element) {
}

#include "SurfaceParameterWidget.moc"
