#include "widgets/world/AbstractParameterWidget.h"
#include "world/objects/Element.h"

#include <QVariant>

struct AbstractParameterWidget::Private {
  Private() : element(0) {}
  
  Element* element;
  QString parameterName;
};

AbstractParameterWidget::AbstractParameterWidget(QWidget* parent)
  : QWidget(parent), p(std::make_unique<Private>())
{
}

AbstractParameterWidget::~AbstractParameterWidget() {
}

void AbstractParameterWidget::parameterChanged() {
  emit changed(p->parameterName, value());
}

void AbstractParameterWidget::setElement(Element* element) {
  p->element = element;
}

void AbstractParameterWidget::setParameterName(const QString& name) {
  p->parameterName = name;
}

#include "AbstractParameterWidget.moc"
