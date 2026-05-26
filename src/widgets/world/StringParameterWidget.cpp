#include "widgets/world/StringParameterWidget.h"
#include "ui_StringParameterWidget.h"

#include <QSignalBlocker>

struct StringParameterWidget::Private {
  Ui::StringParameterWidget ui;
};

StringParameterWidget::StringParameterWidget(QWidget* parent)
    : AbstractParameterWidget(parent),
      p(std::make_unique<Private>()) {
  p->ui.setupUi(this);
  connect(p->ui.stringEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
}

StringParameterWidget::~StringParameterWidget() {
}

void StringParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->ui.label->setText(displayNameForParameter(name));
}

const QVariant StringParameterWidget::value() const {
  return QVariant::fromValue(p->ui.stringEdit->text());
}

void StringParameterWidget::setValue(const QVariant& value) {
  const QSignalBlocker blocker(p->ui.stringEdit);
  p->ui.stringEdit->setText(value.toString());
}
