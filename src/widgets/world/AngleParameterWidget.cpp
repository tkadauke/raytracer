#include "widgets/world/AngleParameterWidget.h"
#include "AngleParameterWidget.uic"

#include "core/math/Angle.h"

Q_DECLARE_METATYPE(Angled);

struct AngleParameterWidget::Private {
  Ui::AngleParameterWidget ui;
};

AngleParameterWidget::AngleParameterWidget(QWidget* parent)
  : AbstractParameterWidget(parent), p(std::make_unique<Private>())
{
  p->ui.setupUi(this);
  connect(p->ui.m_angleEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  connect(p->ui.m_angleType, SIGNAL(currentTextChanged(const QString&)), this, SLOT(recalculate()));
}

AngleParameterWidget::~AngleParameterWidget() {
}

void AngleParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->ui.m_label->setText(name);
}

const QVariant AngleParameterWidget::value() {
  double editValue = p->ui.m_angleEdit->text().toDouble();
  
  Angled angle;
  if (type() == "Degrees") {
    angle = Angled::fromDegrees(editValue);
  } else if (type() == "Turns") {
    angle = Angled::fromTurns(editValue);
  } else {
    angle = Angled::fromRadians(editValue);
  }
  return QVariant::fromValue(angle);
}

void AngleParameterWidget::setValue(const QVariant& value) {
  auto angle = value.value<Angled>();
  
  double number;
  if (type() == "Degrees") {
    number = angle.degrees();
  } else if (type() == "Turns") {
    number = angle.turns();
  } else {
    number = angle.radians();
  }
  
  p->ui.m_angleEdit->setText(QString::number(number));
}

void AngleParameterWidget::recalculate() {
  setValue(lastValue());
}

QString AngleParameterWidget::type() {
  return p->ui.m_angleType->currentText();
}

#include "AngleParameterWidget.moc"
