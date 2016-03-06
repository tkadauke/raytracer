#include "widgets/world/AbstractParameterWidget.h"
#include "world/objects/Element.h"

#include <QVariant>

struct AbstractParameterWidget::Private {
  Private()
    : element(0)
  {
  }
  
  Element* element;
  QString parameterName;
  QVariant lastValue;
};

AbstractParameterWidget::AbstractParameterWidget(QWidget* parent)
  : QWidget(parent),
    p(std::make_unique<Private>())
{
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
}

const QString& AbstractParameterWidget::parameterName() const {
  return p->parameterName;
}

void AbstractParameterWidget::setParameterName(const QString& name) {
  p->parameterName = name;
}

#include "AbstractParameterWidget.moc"
