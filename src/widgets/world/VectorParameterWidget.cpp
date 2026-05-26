#include "widgets/world/VectorParameterWidget.h"

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>

Q_DECLARE_METATYPE(Vector3d);

struct VectorParameterWidget::Private {
  QLabel* label{nullptr};
  QDoubleSpinBox* xEdit{nullptr};
  QDoubleSpinBox* yEdit{nullptr};
  QDoubleSpinBox* zEdit{nullptr};

  QDoubleSpinBox* makeCoordinateEdit(QWidget* parent) const {
    auto* edit = new QDoubleSpinBox(parent);
    edit->setRange(-1000000.0, 1000000.0);
    edit->setDecimals(4);
    edit->setSingleStep(0.1);
    return edit;
  }
};

VectorParameterWidget::VectorParameterWidget(QWidget* parent)
    : AbstractParameterWidget(parent),
      p(std::make_unique<Private>()) {
  auto* layout = new QGridLayout(this);
  layout->setContentsMargins(6, 2, 6, 2);
  layout->setHorizontalSpacing(6);
  layout->setVerticalSpacing(2);

  p->label = new QLabel(this);
  p->xEdit = p->makeCoordinateEdit(this);
  p->yEdit = p->makeCoordinateEdit(this);
  p->zEdit = p->makeCoordinateEdit(this);

  layout->addWidget(p->label, 0, 0);
  layout->addWidget(new QLabel(QStringLiteral("X"), this), 1, 0);
  layout->addWidget(p->xEdit, 1, 1);
  layout->addWidget(new QLabel(QStringLiteral("Y"), this), 1, 2);
  layout->addWidget(p->yEdit, 1, 3);
  layout->addWidget(new QLabel(QStringLiteral("Z"), this), 1, 4);
  layout->addWidget(p->zEdit, 1, 5);
  layout->setColumnStretch(1, 1);
  layout->setColumnStretch(3, 1);
  layout->setColumnStretch(5, 1);

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

void VectorParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->label->setText(displayNameForParameter(name));
}

const QVariant VectorParameterWidget::value() const {
  return QVariant::fromValue(vector());
}

void VectorParameterWidget::setValue(const QVariant& value) {
  if (p->xEdit->hasFocus() || p->yEdit->hasFocus() || p->zEdit->hasFocus())
    return;

  setVector(value.value<Vector3d>());
}
