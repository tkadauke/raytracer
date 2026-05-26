#include "widgets/world/VectorParameterWidget.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

Q_DECLARE_METATYPE(Vector3d);

struct VectorParameterWidget::Private {
  QLabel* label{nullptr};
  QDoubleSpinBox* xEdit{nullptr};
  QDoubleSpinBox* yEdit{nullptr};
  QDoubleSpinBox* zEdit{nullptr};

  QDoubleSpinBox* makeCoordinateEdit(QWidget* parent) const {
    auto* edit = new QDoubleSpinBox(parent);
    edit->setMinimumWidth(0);
    edit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    edit->setRange(-1000000.0, 1000000.0);
    edit->setDecimals(4);
    edit->setSingleStep(0.1);
    return edit;
  }

  void addCoordinateRow(QVBoxLayout* layout, const QString& name, QDoubleSpinBox* edit,
                        QWidget* parent) const {
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);
    auto* rowLabel = new QLabel(name, parent);
    rowLabel->setFixedWidth(12);
    row->addWidget(rowLabel);
    row->addWidget(edit, 1);
    layout->addLayout(row);
  }
};

VectorParameterWidget::VectorParameterWidget(QWidget* parent)
    : AbstractParameterWidget(parent),
      p(std::make_unique<Private>()) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(2, 2, 2, 2);
  layout->setSpacing(2);

  p->label = new QLabel(this);
  p->label->setWordWrap(true);
  p->label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  p->xEdit = p->makeCoordinateEdit(this);
  p->yEdit = p->makeCoordinateEdit(this);
  p->zEdit = p->makeCoordinateEdit(this);

  layout->addWidget(p->label);
  p->addCoordinateRow(layout, QStringLiteral("X"), p->xEdit, this);
  p->addCoordinateRow(layout, QStringLiteral("Y"), p->yEdit, this);
  p->addCoordinateRow(layout, QStringLiteral("Z"), p->zEdit, this);

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
