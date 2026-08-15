#include "widgets/world/AbstractParameterWidget.h"
#include "world/objects/Element.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QVariant>

struct AbstractParameterWidget::Private {
  Private()
      : element(nullptr),
        lastValue() {
  }

  Element* element;
  QString parameterName;
  QVariant lastValue;
};

AbstractParameterWidget::AbstractParameterWidget(QWidget* parent)
    : QWidget(parent),
      p(std::make_unique<Private>()) {
}

AbstractParameterWidget::~AbstractParameterWidget() {
}

void AbstractParameterWidget::parameterChanged() {
  p->lastValue = value();
  emit changed(p->parameterName, p->lastValue);
}

QVariant AbstractParameterWidget::lastValue() const {
  return p->lastValue;
}

void AbstractParameterWidget::setElement(Element* element) {
  p->element = element;
  if (!p->parameterName.isEmpty())
    setToolTip(p->element ? p->element->propertyDescription(p->parameterName) : QString());
  updatePropertyConfiguration();
}

Element* AbstractParameterWidget::element() const {
  return p->element;
}

const QString& AbstractParameterWidget::parameterName() const {
  return p->parameterName;
}

void AbstractParameterWidget::setParameterName(const QString& name) {
  p->parameterName = name;
  if (p->element) {
    setToolTip(p->element->propertyDescription(name));
  }
  updatePropertyConfiguration();
  setLabelText(displayNameForParameter(name));
}

void AbstractParameterWidget::setLabelText(const QString&) {
}

QString AbstractParameterWidget::displayNameForParameter(const QString& name) const {
  if (!p->element)
    return name;
  return p->element->propertyDisplayName(name);
}

QString AbstractParameterWidget::displayNameForChoice(const QString& choice) const {
  if (!p->element)
    return choice;
  return p->element->propertyChoiceDisplayName(p->parameterName, choice);
}

void AbstractParameterWidget::updatePropertyConfiguration() {
}

QDoubleSpinBox* AbstractParameterWidget::makeSpinBoxEdit(QWidget* parent, double minimum,
                                                         double maximum, int decimals,
                                                         double singleStep) {
  auto* edit = new QDoubleSpinBox(parent);
  edit->setMinimumWidth(0);
  edit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  edit->setRange(minimum, maximum);
  edit->setDecimals(decimals);
  edit->setSingleStep(singleStep);
  return edit;
}

void AbstractParameterWidget::addLabeledSpinBoxRow(QVBoxLayout* layout, const QString& name,
                                                    QDoubleSpinBox* edit, QWidget* parent) {
  auto* row = new QHBoxLayout;
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(4);
  auto* rowLabel = new QLabel(name, parent);
  rowLabel->setFixedWidth(12);
  row->addWidget(rowLabel);
  row->addWidget(edit, 1);
  layout->addLayout(row);
}
