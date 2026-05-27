#include "widgets/world/AbstractParameterWidget.h"
#include "world/objects/Element.h"

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
