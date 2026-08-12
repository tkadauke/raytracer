#include "widgets/world/BoolParameterWidget.h"
#include "ui_BoolParameterWidget.h"

#include <QSizePolicy>

struct BoolParameterWidget::Private {
  Ui::BoolParameterWidget ui;
};

BoolParameterWidget::BoolParameterWidget(QWidget* parent)
    : AbstractParameterWidget(parent),
      p(std::make_unique<Private>()) {
  p->ui.setupUi(this);
  p->ui.checkBox->setMinimumWidth(0);
  p->ui.checkBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  connect(p->ui.checkBox, SIGNAL(clicked()), this, SLOT(parameterChanged()));
}

BoolParameterWidget::~BoolParameterWidget() {
}

void BoolParameterWidget::setLabelText(const QString& text) {
  p->ui.checkBox->setText(text);
}

const QVariant BoolParameterWidget::value() const {
  return QVariant::fromValue(p->ui.checkBox->checkState() == Qt::Checked);
}

void BoolParameterWidget::setValue(const QVariant& value) {
  p->ui.checkBox->setCheckState(value.toBool() ? Qt::Checked : Qt::Unchecked);
}
