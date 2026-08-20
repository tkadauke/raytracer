#include "widgets/world/IntParameterWidget.h"
#include "world/objects/Element.h"
#include "ui_IntParameterWidget.h"

#include <QSignalBlocker>
#include <QSizePolicy>

struct IntParameterWidget::Private {
  Ui::IntParameterWidget ui;
};

IntParameterWidget::IntParameterWidget(QWidget* parent)
    : AbstractParameterWidget(parent),
      p(std::make_unique<Private>()) {
  p->ui.setupUi(this);
  configureLabel(p->ui.label);
  p->ui.intEdit->setMinimumWidth(0);
  p->ui.intEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  connect(p->ui.intEdit, SIGNAL(valueChanged(int)), this, SLOT(parameterChanged()));
}

IntParameterWidget::~IntParameterWidget() {
}

void IntParameterWidget::setLabelText(const QString& text) {
  p->ui.label->setText(text);
}

const QVariant IntParameterWidget::value() const {
  return QVariant::fromValue(p->ui.intEdit->value());
}

void IntParameterWidget::setValue(const QVariant& value) {
  if (anyHasFocus(p->ui.intEdit))
    return;
  const QSignalBlocker blocker(p->ui.intEdit);
  p->ui.intEdit->setValue(value.toInt());
}

void IntParameterWidget::updatePropertyConfiguration() {
  if (!element())
    return;

  const auto range = element()->propertyIntRange(parameterName());
  if (!range)
    return;

  p->ui.intEdit->setRange(range->first, range->second);
}
