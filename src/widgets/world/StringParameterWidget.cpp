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
  connect(p->ui.m_stringEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
}

StringParameterWidget::~StringParameterWidget() {
}

void StringParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->ui.m_label->setText(name);
}

const QVariant StringParameterWidget::value() {
  return QVariant::fromValue(p->ui.m_stringEdit->text());
}

void StringParameterWidget::setValue(const QVariant& value) {
  p->ui.m_stringEdit->setText(value.toString());
}

#include "StringParameterWidget.moc"
