#include "widgets/world/BoolParameterWidget.h"
#include "BoolParameterWidget.uic"

struct BoolParameterWidget::Private {
  Ui::BoolParameterWidget ui;
};

BoolParameterWidget::BoolParameterWidget(QWidget* parent)
  : AbstractParameterWidget(parent),
    p(std::make_unique<Private>())
{
  p->ui.setupUi(this);
  connect(p->ui.checkBox, SIGNAL(clicked()), this, SLOT(parameterChanged()));
}

BoolParameterWidget::~BoolParameterWidget() {
}

void BoolParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->ui.checkBox->setText(name);
}

const QVariant BoolParameterWidget::value() const {
  return QVariant::fromValue(p->ui.checkBox->checkState() == Qt::Checked);
}

void BoolParameterWidget::setValue(const QVariant& value) {
  p->ui.checkBox->setCheckState(value.toBool() ? Qt::Checked : Qt::Unchecked);
}

#include "BoolParameterWidget.moc"
