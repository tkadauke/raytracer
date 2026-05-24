#include "widgets/world/PropertyEditorWidget.h"
#include "world/objects/Element.h"
#include "widgets/world/AbstractParameterWidget.h"

#include "widgets/world/VectorParameterWidget.h"
#include "widgets/world/AngleParameterWidget.h"
#include "widgets/world/ColorParameterWidget.h"
#include "widgets/world/StringParameterWidget.h"
#include "widgets/world/IntParameterWidget.h"
#include "widgets/world/DoubleParameterWidget.h"
#include "widgets/world/BoolParameterWidget.h"
#include "widgets/world/ReferenceParameterWidget.h"

#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QVBoxLayout>
#include <QMetaProperty>
#include <QTreeWidget>
#include <QTreeWidgetItem>

Q_DECLARE_METATYPE(Vector3d)

namespace {

  bool isType(const QString& actual, const char* qt5Name, const char* qt6Name) {
    return actual == qt5Name || actual == qt6Name;
  }

} // namespace

struct PropertyEditorWidget::Private {
  inline Private()
      : root(nullptr),
        element(nullptr),
        verticalLayout(nullptr) {
  }

  Element* root;
  Element* element;
  QVBoxLayout* verticalLayout;
  QList<AbstractParameterWidget*> parameterWidgets;
  QList<QWidget*> readOnlyWidgets;
};

PropertyEditorWidget::PropertyEditorWidget(Element* root, QWidget* parent)
    : QWidget(parent),
      p(std::make_unique<Private>()) {
  p->root = root;
}

PropertyEditorWidget::~PropertyEditorWidget() {
  clearParameterWidgets();
  clearReadOnlyWidgets();
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

void PropertyEditorWidget::setReadOnlyProperties(const QString& title,
                                                 const QVector<QPair<QString, QString>>& rows) {
  p->element = nullptr;

  clearParameterWidgets();
  clearReadOnlyWidgets();
  initLayout();

  auto* titleLabel = new QLabel(title, this);
  titleLabel->setObjectName("propertyEditorReadOnlyTitle");
  titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  QFont font = titleLabel->font();
  font.setBold(true);
  titleLabel->setFont(font);

  auto* tree = new QTreeWidget(this);
  tree->setObjectName("propertyEditorReadOnlyProperties");
  tree->setRootIsDecorated(false);
  tree->setAlternatingRowColors(true);
  tree->setHeaderLabels({tr("Property"), tr("Value")});
  tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  tree->header()->setStretchLastSection(true);

  for (const auto& row : rows) {
    auto* item = new QTreeWidgetItem(tree);
    item->setText(0, row.first);
    item->setText(1, row.second.isEmpty() ? QStringLiteral("-") : row.second);
  }

  p->readOnlyWidgets << titleLabel << tree;
  p->verticalLayout->addWidget(titleLabel);
  p->verticalLayout->addWidget(tree, 1);
}

void PropertyEditorWidget::setElement(Element* element) {
  p->element = element;

  clearParameterWidgets();
  clearReadOnlyWidgets();
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

  if (isType(type, "Vector3d", "Vector3<double>")) {
    widget = new VectorParameterWidget(this);
  } else if (isType(type, "Angled", "Angle<double>")) {
    widget = new AngleParameterWidget(this);
  } else if (isType(type, "Colord", "Color<double>")) {
    widget = new ColorParameterWidget(this);
  } else if (type == "QString") {
    widget = new StringParameterWidget(this);
  } else if (type == "int") {
    widget = new IntParameterWidget(this);
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

  connect(widget, SIGNAL(changed(const QString&, const QVariant&)), this,
          SLOT(elementChanged(const QString&, const QVariant&)));
}

void PropertyEditorWidget::clearParameterWidgets() {
  for (const auto& widget : p->parameterWidgets) {
    delete widget;
  }

  p->parameterWidgets.clear();
}

void PropertyEditorWidget::clearReadOnlyWidgets() {
  for (auto* widget : p->readOnlyWidgets) {
    delete widget;
  }

  p->readOnlyWidgets.clear();
}

void PropertyEditorWidget::elementChanged(const QString& propertyName, const QVariant& value) {
  p->element->setProperty(propertyName.toStdString().c_str(), value);
  emit changed(p->element);
}
