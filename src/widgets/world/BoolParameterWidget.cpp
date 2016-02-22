#include "widgets/world/BoolParameterWidget.h"
#include "BoolParameterWidget.uic"

struct BoolParameterWidget::Private {
  Ui::BoolParameterWidget ui;
};

BoolParameterWidget::BoolParameterWidget(QWidget* parent)
  : AbstractParameterWidget(parent), p(std::make_unique<Private>())
{
  p->ui.setupUi(this);
  connect(p->ui.m_checkBox, SIGNAL(clicked()), this, SLOT(parameterChanged()));
}

BoolParameterWidget::~BoolParameterWidget() {
}

void BoolParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->ui.m_checkBox->setText(name);
}

const QVariant BoolParameterWidget::value() {
  return QVariant::fromValue(p->ui.m_checkBox->checkState() == Qt::Checked);
}

void BoolParameterWidget::setValue(const QVariant& value) {
  p->ui.m_checkBox->setCheckState(value.toBool() ? Qt::Checked : Qt::Unchecked);
}

#include "BoolParameterWidget.moc"
