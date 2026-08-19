#include "widgets/world/VectorParameterWidget.h"

#include <QDoubleSpinBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

Q_DECLARE_METATYPE(Vector3d);

struct VectorParameterWidget::Private {
  QLabel* label{nullptr};
  QDoubleSpinBox* xEdit{nullptr};
  QDoubleSpinBox* yEdit{nullptr};
  QDoubleSpinBox* zEdit{nullptr};
};

VectorParameterWidget::VectorParameterWidget(QWidget* parent)
    : AbstractParameterWidget(parent),
      p(std::make_unique<Private>()) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(2, 2, 2, 2);
  layout->setSpacing(2);

  p->label = new QLabel(this);
  configureLabel(p->label);
  p->xEdit = makeSpinBoxEdit(this, -1000000.0, 1000000.0, 4, 0.1);
  p->yEdit = makeSpinBoxEdit(this, -1000000.0, 1000000.0, 4, 0.1);
  p->zEdit = makeSpinBoxEdit(this, -1000000.0, 1000000.0, 4, 0.1);

  layout->addWidget(p->label);
  addLabeledSpinBoxRow(layout, QStringLiteral("X"), p->xEdit, this);
  addLabeledSpinBoxRow(layout, QStringLiteral("Y"), p->yEdit, this);
  addLabeledSpinBoxRow(layout, QStringLiteral("Z"), p->zEdit, this);

  connect(p->xEdit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &VectorParameterWidget::parameterChanged);
  connect(p->yEdit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &VectorParameterWidget::parameterChanged);
  connect(p->zEdit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &VectorParameterWidget::parameterChanged);
}

VectorParameterWidget::~VectorParameterWidget() {
}

Vector3d VectorParameterWidget::vector() const {
  return Vector3d(p->xEdit->value(), p->yEdit->value(), p->zEdit->value());
}

void VectorParameterWidget::setVector(const Vector3d& vector) {
  const QSignalBlocker blockX(p->xEdit);
  const QSignalBlocker blockY(p->yEdit);
  const QSignalBlocker blockZ(p->zEdit);
  p->xEdit->setValue(vector.x());
  p->yEdit->setValue(vector.y());
  p->zEdit->setValue(vector.z());
}

void VectorParameterWidget::setLabelText(const QString& text) {
  p->label->setText(text);
}

const QVariant VectorParameterWidget::value() const {
  return QVariant::fromValue(vector());
}

void VectorParameterWidget::setValue(const QVariant& value) {
  if (p->xEdit->hasFocus() || p->yEdit->hasFocus() || p->zEdit->hasFocus())
    return;

  setVector(value.value<Vector3d>());
}
