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
  Private()
    : element(0), verticalLayout(0) {}
  
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

    auto prop = p->element->property(metaProp.name());

    AbstractParameterWidget* widget = nullptr;

    QString name = QString(metaProp.typeName());

    if (name == "Vector3d") {
      widget = new VectorParameterWidget(this);
    } else if (name == "Angled") {
      widget = new AngleParameterWidget(this);
    } else if (name == "Colord") {
      widget = new ColorParameterWidget(this);
    } else if (name == "QString") {
      widget = new StringParameterWidget(this);
    } else if (name == "double") {
      widget = new DoubleParameterWidget(this);
    } else if (name == "bool") {
      widget = new BoolParameterWidget(this);
    } else if (name == "Material*") {
      widget = new ReferenceParameterWidget("Material", p->root, this);
    } else if (name == "Texture*") {
      widget = new ReferenceParameterWidget("Texture", p->root, this);
    }

    if (widget) {
      widget->setElement(p->element);
      widget->setParameterName(metaProp.name());
      widget->setValue(prop);
      
      addParameterWidget(widget);
    }
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
