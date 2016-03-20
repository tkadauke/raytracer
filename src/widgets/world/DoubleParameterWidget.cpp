#include "widgets/world/DoubleParameterWidget.h"
#include "DoubleParameterWidget.uic"

struct DoubleParameterWidget::Private {
  Ui::DoubleParameterWidget ui;
};

DoubleParameterWidget::DoubleParameterWidget(QWidget* parent)
  : AbstractParameterWidget(parent),
    p(std::make_unique<Private>())
{
  p->ui.setupUi(this);
  connect(p->ui.doubleEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
}

DoubleParameterWidget::~DoubleParameterWidget() {
}

void DoubleParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->ui.label->setText(name);
}

const QVariant DoubleParameterWidget::value() const {
  return QVariant::fromValue(p->ui.doubleEdit->text().toDouble());
}

void DoubleParameterWidget::setValue(const QVariant& value) {
  if (p->ui.doubleEdit->hasFocus())
    return;
  p->ui.doubleEdit->setText(QString::number(value.toDouble()));
}

#include "DoubleParameterWidget.moc"
