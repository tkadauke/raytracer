#include "widgets/world/PropertyEditorWidget.h"
#include "world/objects/Element.h"
#include "widgets/world/AbstractParameterWidget.h"

#include "widgets/world/VectorParameterWidget.h"
#include "widgets/world/AngleParameterWidget.h"
#include "widgets/world/ColorParameterWidget.h"
#include "widgets/world/StringParameterWidget.h"
#include "widgets/world/DoubleParameterWidget.h"
#include "widgets/world/BoolParameterWidget.h"
#include "widgets/world/ReferenceParameterWidget.h"

#include <QVBoxLayout>
#include <QMetaProperty>

Q_DECLARE_METATYPE(Vector3d)

struct PropertyEditorWidget::Private {
  inline Private()
    : root(nullptr),
      element(nullptr),
      verticalLayout(nullptr)
  {
  }
  
  Element* root;
  Element* element;
  QVBoxLayout* verticalLayout;
  QList<AbstractParameterWidget*> parameterWidgets;
};

PropertyEditorWidget::PropertyEditorWidget(Element* root, QWidget* parent)
  : QWidget(parent), p(std::make_unique<Private>())
{
  p->root = root;
}

PropertyEditorWidget::~PropertyEditorWidget() {
  clearParameterWidgets();
}

void PropertyEditorWidget::initLayout() {
  if (p->verticalLayout) {
    delete p->verticalLayout;
  }
  p->verticalLayout = new QVBoxLayout(this);
  p->verticalLayout->setContentsMargins(0, 0, 0, 0);
  p->verticalLayout->setSpacing(0);
}

QSize PropertyEditorWidget::sizeHint() const {
  return QSize(256, 100);
}

void PropertyEditorWidget::setRoot(Element* root) {
  p->root = root;
  setElement(nullptr);
}

void PropertyEditorWidget::setElement(Element* element) {
  p->element = element;
  
  clearParameterWidgets();
  if (p->element) {
    addParameterWidgets();
  }
}

void PropertyEditorWidget::update() {
  for (const auto& widget : p->parameterWidgets) {
    const QString& parameterName = widget->parameterName();
    auto prop = p->element->property(parameterName.toStdString().c_str());
    
    widget->setValue(prop);
  }
}

void PropertyEditorWidget::addParameterWidgets() {
  initLayout();
  addParametersForClass(p->element->metaObject());

  for (const auto& name : p->element->dynamicPropertyNames()) {
    addParameter(name);
  }

  p->verticalLayout->addStretch();
}

void PropertyEditorWidget::addParametersForClass(const QMetaObject* klass) {
  if (klass->className() != QString("Element") && klass->superClass()) {
    addParametersForClass(klass->superClass());
  }

  // TODO: add header

  for (int i = klass->propertyOffset(); i != klass->propertyCount(); ++i) {
    auto metaProp = klass->property(i);
    if (metaProp.name() == QString("id"))
      continue;

    addParameter(metaProp.name());
  }
}

void PropertyEditorWidget::addParameter(const QString& name) {
  auto value = p->element->property(name.toStdString().c_str());
  auto type = QString(value.typeName());

  AbstractParameterWidget* widget = nullptr;

  if (type == "Vector3d") {
    widget = new VectorParameterWidget(this);
  } else if (type == "Angled") {
    widget = new AngleParameterWidget(this);
  } else if (type == "Colord") {
    widget = new ColorParameterWidget(this);
  } else if (type == "QString") {
    widget = new StringParameterWidget(this);
  } else if (type == "double") {
    widget = new DoubleParameterWidget(this);
  } else if (type == "bool") {
    widget = new BoolParameterWidget(this);
  } else if (type == "Material*") {
    widget = new ReferenceParameterWidget("Material", p->root, this);
  } else if (type == "Texture*") {
    widget = new ReferenceParameterWidget("Texture", p->root, this);
  }

  if (widget) {
    widget->setElement(p->element);
    widget->setParameterName(name);
    widget->setValue(value);
    
    addParameterWidget(widget);
  }
}

void PropertyEditorWidget::addParameterWidget(AbstractParameterWidget* widget) {
  p->parameterWidgets << widget;
  p->verticalLayout->addWidget(widget);
  widget->setElement(p->element);
  
  connect(widget, SIGNAL(changed(const QString&, const QVariant&)), this, SLOT(elementChanged(const QString&, const QVariant&)));
}

void PropertyEditorWidget::clearParameterWidgets() {
  for (const auto& widget : p->parameterWidgets) {
    delete widget;
  }
  
  p->parameterWidgets.clear();
}

void PropertyEditorWidget::elementChanged(const QString& propertyName, const QVariant& value) {
  p->element->setProperty(propertyName.toStdString().c_str(), value);
  emit changed(p->element);
}

#include "PropertyEditorWidget.moc"
