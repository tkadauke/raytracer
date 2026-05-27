#include "widgets/world/StringParameterWidget.h"
#include "ui_StringParameterWidget.h"

#include <QSignalBlocker>
#include <QSizePolicy>

struct StringParameterWidget::Private {
  Ui::StringParameterWidget ui;
};

StringParameterWidget::StringParameterWidget(QWidget* parent)
    : AbstractParameterWidget(parent),
      p(std::make_unique<Private>()) {
  p->ui.setupUi(this);
  p->ui.label->setMinimumWidth(0);
  p->ui.label->setWordWrap(true);
  p->ui.label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  p->ui.stringEdit->setMinimumWidth(0);
  p->ui.stringEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
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
  const QString text = value.toString();
  if (p->ui.stringEdit->text() == text)
    return;
  if (p->ui.stringEdit->hasFocus())
    return;
  const QSignalBlocker blocker(p->ui.stringEdit);
  p->ui.stringEdit->setText(text);
}
