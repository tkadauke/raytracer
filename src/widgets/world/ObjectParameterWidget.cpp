#include "widgets/world/ObjectParameterWidget.h"
#include "ObjectParameterWidget.uic"
#include "world/objects/Object.h"

struct ObjectParameterWidget::Private {
  Ui::ObjectParameterWidget ui;
};

ObjectParameterWidget::ObjectParameterWidget(QWidget* parent)
  : QWidget(parent), p(new Private)
{
  p->ui.setupUi(this);
  connect(p->ui.m_nameEdit, SIGNAL(textChanged(const QString&)), this, SLOT(parameterChanged()));
}

ObjectParameterWidget::~ObjectParameterWidget() {
  delete p;
}

void ObjectParameterWidget::parameterChanged() {
  emit changed();
}

std::string ObjectParameterWidget::name() const {
  return p->ui.m_nameEdit->text().toStdString();
}

void ObjectParameterWidget::setName(const std::string& name) {
  p->ui.m_nameEdit->setText(QString::fromStdString(name));
}

void ObjectParameterWidget::applyTo(Object* object) {
  object->setName(name());
}

void ObjectParameterWidget::getFrom(Object* object) {
  setName(object->name());
}

#include "ObjectParameterWidget.moc"
