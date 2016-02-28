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
  connect(p->ui.m_doubleEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
}

DoubleParameterWidget::~DoubleParameterWidget() {
}

void DoubleParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->ui.m_label->setText(name);
}

const QVariant DoubleParameterWidget::value() {
  return QVariant::fromValue(p->ui.m_doubleEdit->text().toDouble());
}

void DoubleParameterWidget::setValue(const QVariant& value) {
  p->ui.m_doubleEdit->setText(QString::number(value.toDouble()));
}

#include "DoubleParameterWidget.moc"
