#include "widgets/world/ElementParameterWidget.h"
#include "ElementParameterWidget.uic"
#include "world/objects/Element.h"

struct ElementParameterWidget::Private {
  Ui::ElementParameterWidget ui;
};

ElementParameterWidget::ElementParameterWidget(QWidget* parent)
  : QWidget(parent), p(new Private)
{
  p->ui.setupUi(this);
  connect(p->ui.m_nameEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
}

ElementParameterWidget::~ElementParameterWidget() {
  delete p;
}

void ElementParameterWidget::parameterChanged() {
  emit changed();
}

QString ElementParameterWidget::name() const {
  return p->ui.m_nameEdit->text();
}

void ElementParameterWidget::setName(const QString& name) {
  p->ui.m_nameEdit->setText(name);
}

void ElementParameterWidget::applyTo(Element* object) {
  object->setName(name());
}

void ElementParameterWidget::getFrom(Element* object) {
  setName(object->name());
}

#include "ElementParameterWidget.moc"
