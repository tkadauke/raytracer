#include "widgets/world/ColorParameterWidget.h"

#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

Q_DECLARE_METATYPE(Colord);

struct ColorParameterWidget::Private {
  QLabel* label{nullptr};
  QDoubleSpinBox* rEdit{nullptr};
  QDoubleSpinBox* gEdit{nullptr};
  QDoubleSpinBox* bEdit{nullptr};
  QToolButton* selectorButton{nullptr};
};

ColorParameterWidget::ColorParameterWidget(QWidget* parent)
    : AbstractParameterWidget(parent),
      p(std::make_unique<Private>()) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(2, 2, 2, 2);
  layout->setSpacing(2);

  p->label = new QLabel(this);
  p->label->setWordWrap(true);
  p->label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  p->rEdit = makeSpinBoxEdit(this, 0.0, 1000000.0, 3, 0.01);
  p->gEdit = makeSpinBoxEdit(this, 0.0, 1000000.0, 3, 0.01);
  p->bEdit = makeSpinBoxEdit(this, 0.0, 1000000.0, 3, 0.01);
  p->selectorButton = new QToolButton(this);
  p->selectorButton->setToolTip(tr("Select color"));
  p->selectorButton->setFixedSize(24, 20);

  auto* header = new QHBoxLayout;
  header->setContentsMargins(0, 0, 0, 0);
  header->setSpacing(4);
  header->addWidget(p->label, 1);
  header->addWidget(p->selectorButton);
  layout->addLayout(header);
  addLabeledSpinBoxRow(layout, QStringLiteral("R"), p->rEdit, this);
  addLabeledSpinBoxRow(layout, QStringLiteral("G"), p->gEdit, this);
  addLabeledSpinBoxRow(layout, QStringLiteral("B"), p->bEdit, this);

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

void ColorParameterWidget::setLabelText(const QString& text) {
  p->label->setText(text);
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
