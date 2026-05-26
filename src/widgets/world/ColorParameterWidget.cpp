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

  QDoubleSpinBox* makeChannelEdit(QWidget* parent) const {
    auto* edit = new QDoubleSpinBox(parent);
    edit->setMinimumWidth(0);
    edit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    edit->setRange(0.0, 1000000.0);
    edit->setDecimals(3);
    edit->setSingleStep(0.01);
    return edit;
  }

  void addChannelRow(QVBoxLayout* layout, const QString& name, QDoubleSpinBox* edit,
                     QWidget* parent) const {
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);
    auto* rowLabel = new QLabel(name, parent);
    rowLabel->setFixedWidth(12);
    row->addWidget(rowLabel);
    row->addWidget(edit, 1);
    layout->addLayout(row);
  }
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
  p->rEdit = p->makeChannelEdit(this);
  p->gEdit = p->makeChannelEdit(this);
  p->bEdit = p->makeChannelEdit(this);
  p->selectorButton = new QToolButton(this);
  p->selectorButton->setToolTip(tr("Select color"));
  p->selectorButton->setFixedSize(24, 20);

  auto* header = new QHBoxLayout;
  header->setContentsMargins(0, 0, 0, 0);
  header->setSpacing(4);
  header->addWidget(p->label, 1);
  header->addWidget(p->selectorButton);
  layout->addLayout(header);
  p->addChannelRow(layout, QStringLiteral("R"), p->rEdit, this);
  p->addChannelRow(layout, QStringLiteral("G"), p->gEdit, this);
  p->addChannelRow(layout, QStringLiteral("B"), p->bEdit, this);

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
