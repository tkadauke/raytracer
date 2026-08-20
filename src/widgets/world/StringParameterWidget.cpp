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
  configureLabel(p->ui.label);
  p->ui.stringEdit->setMinimumWidth(0);
  p->ui.stringEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  connect(p->ui.stringEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
}

StringParameterWidget::~StringParameterWidget() {
}

void StringParameterWidget::setLabelText(const QString& text) {
  p->ui.label->setText(text);
}

const QVariant StringParameterWidget::value() const {
  return QVariant::fromValue(p->ui.stringEdit->text());
}

void StringParameterWidget::setValue(const QVariant& value) {
  const QString text = value.toString();
  if (p->ui.stringEdit->text() == text)
    return;
  if (anyHasFocus(p->ui.stringEdit))
    return;
  const QSignalBlocker blocker(p->ui.stringEdit);
  p->ui.stringEdit->setText(text);
}
