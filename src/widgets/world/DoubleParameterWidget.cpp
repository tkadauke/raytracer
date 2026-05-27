#include "widgets/world/DoubleParameterWidget.h"
#include "world/objects/Element.h"

#include <QDoubleSpinBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {
  int decimalsForStep(double step) {
    if (!std::isfinite(step) || step <= 0.0)
      return 6;

    QString text = QString::number(std::abs(step), 'f', 12);
    while (text.contains(QChar('.')) && text.endsWith(QChar('0')))
      text.chop(1);
    const int dot = text.indexOf(QChar('.'));
    return dot < 0 ? 0 : std::max(0, static_cast<int>(text.size() - dot - 1));
  }
}

struct DoubleParameterWidget::Private {
  QLabel* label{nullptr};
  QDoubleSpinBox* doubleEdit{nullptr};
};

DoubleParameterWidget::DoubleParameterWidget(QWidget* parent)
    : AbstractParameterWidget(parent),
      p(std::make_unique<Private>()) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(2, 2, 2, 2);
  layout->setSpacing(2);

  p->label = new QLabel(this);
  p->label->setWordWrap(true);
  p->label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  p->doubleEdit = new QDoubleSpinBox(this);
  p->doubleEdit->setMinimumWidth(0);
  p->doubleEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  p->doubleEdit->setRange(-1000000.0, 1000000.0);
  p->doubleEdit->setDecimals(6);
  p->doubleEdit->setSingleStep(0.1);

  layout->addWidget(p->label);
  layout->addWidget(p->doubleEdit);

  connect(p->doubleEdit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &DoubleParameterWidget::parameterChanged);
}

DoubleParameterWidget::~DoubleParameterWidget() {
}

void DoubleParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->label->setText(displayNameForParameter(name));
}

const QVariant DoubleParameterWidget::value() const {
  return QVariant::fromValue(p->doubleEdit->value());
}

void DoubleParameterWidget::setValue(const QVariant& value) {
  if (p->doubleEdit->hasFocus())
    return;
  const QSignalBlocker blocker(p->doubleEdit);
  p->doubleEdit->setValue(value.toDouble());
}

void DoubleParameterWidget::updatePropertyConfiguration() {
  if (!element())
    return;

  const auto range = element()->propertyDoubleRange(parameterName());
  if (range)
    p->doubleEdit->setRange(range->first, range->second);

  const auto step = element()->propertyDoubleStep(parameterName());
  if (step && *step > 0.0) {
    p->doubleEdit->setDecimals(decimalsForStep(*step));
    p->doubleEdit->setSingleStep(*step);
  }
}
