#include "widgets/world/AngleParameterWidget.h"
#include "AngleParameterWidget.uic"

#include "core/math/Angle.h"

using namespace std;

Q_DECLARE_METATYPE(Angled);

struct AngleParameterWidget::Private {
  Ui::AngleParameterWidget ui;
};

AngleParameterWidget::AngleParameterWidget(QWidget* parent)
  : AbstractParameterWidget(parent),
    p(std::make_unique<Private>())
{
  p->ui.setupUi(this);
  connect(p->ui.angleEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  connect(p->ui.angleType, SIGNAL(currentTextChanged(const QString&)), this, SLOT(recalculate()));
}

AngleParameterWidget::~AngleParameterWidget() {
}

void AngleParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->ui.label->setText(name);
}

const QVariant AngleParameterWidget::value() const {
  double editValue = p->ui.angleEdit->text().toDouble();
  
  typedef Angled (*funcptr)(const double&);
  static auto fromMap = map<string, funcptr>({
    {"Radians", &Angled::fromRadians},
    {"Grads", &Angled::fromGrads},
    {"Degrees", &Angled::fromDegrees},
    {"Arc minutes", &Angled::fromArcMinutes},
    {"Arc seconds", &Angled::fromArcSeconds},
    {"Hexacontades", &Angled::fromHexacontades},
    {"Binary degrees", &Angled::fromBinaryDegrees},
    {"Turns", &Angled::fromTurns},
    {"Quadrants", &Angled::fromQuadrants},
    {"Sextants", &Angled::fromSextants},
    {"o'Clock", &Angled::fromClock},
    {"Hours", &Angled::fromHours},
    {"Compass points", &Angled::fromCompassPoints}
  });

  Angled angle = fromMap[type().toStdString()](editValue);
  return QVariant::fromValue(angle);
}

void AngleParameterWidget::setValue(const QVariant& value) {
  if (p->ui.angleEdit->hasFocus())
    return;
  
  auto angle = value.value<Angled>();
  typedef double (Angled::*funcptr)() const;
  static auto toMap = map<string, funcptr>({
    {"Radians", &Angled::radians},
    {"Grads", &Angled::grads},
    {"Degrees", &Angled::degrees},
    {"Arc minutes", &Angled::arcMinutes},
    {"Arc seconds", &Angled::arcSeconds},
    {"Hexacontades", &Angled::hexacontades},
    {"Binary degrees", &Angled::binaryDegrees},
    {"Turns", &Angled::turns},
    {"Quadrants", &Angled::quadrants},
    {"Sextants", &Angled::sextants},
    {"o'Clock", &Angled::oclock},
    {"Hours", &Angled::hours},
    {"Compass points", &Angled::compassPoints},
  });

  funcptr fp = toMap[type().toStdString()];
  double number = (angle.*fp)();
  p->ui.angleEdit->setText(QString::number(number));
}

void AngleParameterWidget::recalculate() {
  setValue(lastValue());
}

QString AngleParameterWidget::type() const {
  return p->ui.angleType->currentText();
}

#include "AngleParameterWidget.moc"
