#include "widgets/world/StringParameterWidget.h"
#include "StringParameterWidget.uic"

struct StringParameterWidget::Private {
  Ui::StringParameterWidget ui;
};

StringParameterWidget::StringParameterWidget(QWidget* parent)
  : AbstractParameterWidget(parent),
    p(std::make_unique<Private>())
{
  p->ui.setupUi(this);
  connect(p->ui.stringEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
}

StringParameterWidget::~StringParameterWidget() {
}

void StringParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->ui.label->setText(name);
}

const QVariant StringParameterWidget::value() const {
  return QVariant::fromValue(p->ui.stringEdit->text());
}

void StringParameterWidget::setValue(const QVariant& value) {
  p->ui.stringEdit->setText(value.toString());
}

#include "StringParameterWidget.moc"
