#include "widgets/world/ColorParameterWidget.h"

#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QToolButton>

Q_DECLARE_METATYPE(Colord);

struct ColorParameterWidget::Private {
  QLabel* label{nullptr};
  QDoubleSpinBox* rEdit{nullptr};
  QDoubleSpinBox* gEdit{nullptr};
  QDoubleSpinBox* bEdit{nullptr};
  QToolButton* selectorButton{nullptr};

  QDoubleSpinBox* makeChannelEdit(QWidget* parent) const {
    auto* edit = new QDoubleSpinBox(parent);
    edit->setRange(0.0, 1000000.0);
    edit->setDecimals(3);
    edit->setSingleStep(0.01);
    return edit;
  }
};

ColorParameterWidget::ColorParameterWidget(QWidget* parent)
    : AbstractParameterWidget(parent),
      p(std::make_unique<Private>()) {
  auto* layout = new QGridLayout(this);
  layout->setContentsMargins(6, 2, 6, 2);
  layout->setHorizontalSpacing(6);
  layout->setVerticalSpacing(2);

  p->label = new QLabel(this);
  p->rEdit = p->makeChannelEdit(this);
  p->gEdit = p->makeChannelEdit(this);
  p->bEdit = p->makeChannelEdit(this);
  p->selectorButton = new QToolButton(this);
  p->selectorButton->setToolTip(tr("Select color"));
  p->selectorButton->setMinimumWidth(32);

  layout->addWidget(p->label, 0, 0);
  layout->addWidget(new QLabel(QStringLiteral("R"), this), 1, 0);
  layout->addWidget(p->rEdit, 1, 1);
  layout->addWidget(new QLabel(QStringLiteral("G"), this), 1, 2);
  layout->addWidget(p->gEdit, 1, 3);
  layout->addWidget(new QLabel(QStringLiteral("B"), this), 1, 4);
  layout->addWidget(p->bEdit, 1, 5);
  layout->addWidget(p->selectorButton, 1, 6);
  layout->setColumnStretch(1, 1);
  layout->setColumnStretch(3, 1);
  layout->setColumnStretch(5, 1);

  connect(p->rEdit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &ColorParameterWidget::parameterChanged);
  connect(p->gEdit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &ColorParameterWidget::parameterChanged);
  connect(p->bEdit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &ColorParameterWidget::parameterChanged);

  connect(p->selectorButton, SIGNAL(clicked()), this, SLOT(selectorClicked()));
}

ColorParameterWidget::~ColorParameterWidget() {
}

Colord ColorParameterWidget::color() const {
  return Colord(p->rEdit->value(), p->gEdit->value(), p->bEdit->value());
}

void ColorParameterWidget::setColor(const Colord& color) {
  if (p->rEdit->hasFocus() || p->gEdit->hasFocus() || p->bEdit->hasFocus())
    return;

  const QSignalBlocker blockR(p->rEdit);
  const QSignalBlocker blockG(p->gEdit);
  const QSignalBlocker blockB(p->bEdit);
  p->rEdit->setValue(color.r());
  p->gEdit->setValue(color.g());
  p->bEdit->setValue(color.b());
  p->selectorButton->setStyleSheet(
    QStringLiteral("QToolButton { background-color: %1; border: 1px solid #777; }")
      .arg(colordToQColor(color).name()));
}

void ColorParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->label->setText(displayNameForParameter(name));
}

void ColorParameterWidget::selectorClicked() {
  QColor newColor = QColorDialog::getColor(colordToQColor(color()), this, "Select color");
  if (!newColor.isValid())
    return;
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
