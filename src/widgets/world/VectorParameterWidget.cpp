#include "widgets/world/VectorParameterWidget.h"
#include "VectorParameterWidget.uic"

Q_DECLARE_METATYPE(Vector3d)

struct VectorParameterWidget::Private {
  Ui::VectorParameterWidget ui;
};

VectorParameterWidget::VectorParameterWidget(QWidget* parent)
  : AbstractParameterWidget(parent), p(new Private)
{
  p->ui.setupUi(this);
  connect(p->ui.m_xEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  connect(p->ui.m_yEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  connect(p->ui.m_zEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
}

VectorParameterWidget::~VectorParameterWidget() {
  delete p;
}

Vector3d VectorParameterWidget::vector() const {
  return Vector3d(
    p->ui.m_xEdit->text().toDouble(),
    p->ui.m_yEdit->text().toDouble(),
    p->ui.m_zEdit->text().toDouble()
  );
}

void VectorParameterWidget::setVector(const Vector3d& vector) {
  p->ui.m_xEdit->setText(QString::number(vector.x()));
  p->ui.m_yEdit->setText(QString::number(vector.y()));
  p->ui.m_zEdit->setText(QString::number(vector.z()));
}

void VectorParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->ui.m_label->setText(name);
}

const QVariant VectorParameterWidget::value() {
  return QVariant::fromValue(vector());
}

void VectorParameterWidget::setValue(const QVariant& value) {
  setVector(value.value<Vector3d>());
}

#include "VectorParameterWidget.moc"
