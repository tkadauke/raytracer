#include "widgets/world/ColorParameterWidget.h"
#include "ColorParameterWidget.uic"

#include <QColorDialog>

Q_DECLARE_METATYPE(Colord);

struct ColorParameterWidget::Private {
  Ui::ColorParameterWidget ui;
};

ColorParameterWidget::ColorParameterWidget(QWidget* parent)
  : AbstractParameterWidget(parent),
    p(std::make_unique<Private>())
{
  p->ui.setupUi(this);
  connect(p->ui.rEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  connect(p->ui.gEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  connect(p->ui.bEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
  
  connect(p->ui.selectorButton, SIGNAL(clicked()), this, SLOT(selectorClicked()));
}

ColorParameterWidget::~ColorParameterWidget() {
}

Colord ColorParameterWidget::color() const {
  return Colord(
    p->ui.rEdit->text().toDouble(),
    p->ui.gEdit->text().toDouble(),
    p->ui.bEdit->text().toDouble()
  );
}

void ColorParameterWidget::setColor(const Colord& color) {
  if (p->ui.rEdit->hasFocus() || p->ui.gEdit->hasFocus() || p->ui.bEdit->hasFocus())
    return;
  
  p->ui.rEdit->setText(QString::number(color.r()));
  p->ui.gEdit->setText(QString::number(color.g()));
  p->ui.bEdit->setText(QString::number(color.b()));
}

void ColorParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->ui.label->setText(name);
}

void ColorParameterWidget::selectorClicked() {
  QColor newColor = QColorDialog::getColor(colordToQColor(color()), this, "Select color");
  setColor(qColorToColord(newColor));
  parameterChanged();
}

Colord ColorParameterWidget::qColorToColord(const QColor& color) {
  return Colord::fromRGB(color.red(), color.green(), color.blue());
}

QColor ColorParameterWidget::colordToQColor(const Colord& color) {
  return QColor(color.rInt(), color.gInt(), color.bInt());
}

const QVariant ColorParameterWidget::value() const {
  return QVariant::fromValue(color());
}

void ColorParameterWidget::setValue(const QVariant& value) {
  setColor(value.value<Colord>());
}

#include "ColorParameterWidget.moc"
