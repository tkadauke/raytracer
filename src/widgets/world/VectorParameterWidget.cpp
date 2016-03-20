#include "widgets/world/VectorParameterWidget.h"
#include "VectorParameterWidget.uic"

Q_DECLARE_METATYPE(Vector3d);

struct VectorParameterWidget::Private {
  Ui::VectorParameterWidget ui;
};

VectorParameterWidget::VectorParameterWidget(QWidget* parent)
  : AbstractParameterWidget(parent),
    p(std::make_unique<Private>())
{
  p->ui.setupUi(this);
  connect(p->ui.xEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  connect(p->ui.yEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  connect(p->ui.zEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
}

VectorParameterWidget::~VectorParameterWidget() {
}

Vector3d VectorParameterWidget::vector() const {
  return Vector3d(
    p->ui.xEdit->text().toDouble(),
    p->ui.yEdit->text().toDouble(),
    p->ui.zEdit->text().toDouble()
  );
}

void VectorParameterWidget::setVector(const Vector3d& vector) {
  p->ui.xEdit->setText(QString::number(vector.x()));
  p->ui.yEdit->setText(QString::number(vector.y()));
  p->ui.zEdit->setText(QString::number(vector.z()));
}

void VectorParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->ui.label->setText(name);
}

const QVariant VectorParameterWidget::value() const {
  return QVariant::fromValue(vector());
}

void VectorParameterWidget::setValue(const QVariant& value) {
  if (p->ui.xEdit->hasFocus() || p->ui.yEdit->hasFocus() || p->ui.zEdit->hasFocus())
    return;

  setVector(value.value<Vector3d>());
}

#include "VectorParameterWidget.moc"
