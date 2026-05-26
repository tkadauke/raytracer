#include "widgets/world/IntParameterWidget.h"
#include "ui_IntParameterWidget.h"

#include <QSignalBlocker>

struct IntParameterWidget::Private {
  Ui::IntParameterWidget ui;
};

IntParameterWidget::IntParameterWidget(QWidget* parent)
    : AbstractParameterWidget(parent),
      p(std::make_unique<Private>()) {
  p->ui.setupUi(this);
  connect(p->ui.intEdit, SIGNAL(valueChanged(int)), this, SLOT(parameterChanged()));
}

IntParameterWidget::~IntParameterWidget() {
}

void IntParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->ui.label->setText(displayNameForParameter(name));
}

const QVariant IntParameterWidget::value() const {
  return QVariant::fromValue(p->ui.intEdit->value());
}

void IntParameterWidget::setValue(const QVariant& value) {
  if (p->ui.intEdit->hasFocus())
    return;
  const QSignalBlocker blocker(p->ui.intEdit);
  p->ui.intEdit->setValue(value.toInt());
}
