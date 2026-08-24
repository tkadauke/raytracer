#include "widgets/world/PropertyEditorWidget.h"
#include "world/objects/Element.h"
#include "world/objects/Group.h"
#include "widgets/world/AbstractParameterWidget.h"

#include "widgets/world/VectorParameterWidget.h"
#include "widgets/world/AngleParameterWidget.h"
#include "widgets/world/ChoiceParameterWidget.h"
#include "widgets/world/ColorParameterWidget.h"
#include "widgets/world/StringParameterWidget.h"
#include "widgets/world/IntParameterWidget.h"
#include "widgets/world/DoubleParameterWidget.h"
#include "widgets/world/BoolParameterWidget.h"
#include "widgets/world/ReferenceParameterWidget.h"

#include <QFont>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMetaProperty>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

Q_DECLARE_METATYPE(Vector3d)

namespace {

  QString metadataValueText(const QJsonValue& value) {
    if (value.isUndefined())
      return QStringLiteral("-");

    QJsonArray wrapper;
    wrapper.append(value);
    auto text = QString::fromUtf8(QJsonDocument(wrapper).toJson(QJsonDocument::Compact));
    return text.mid(1, text.size() - 2);
  }

} // namespace

struct PropertyEditorWidget::Private {
  inline Private()
      : root(nullptr),
        element(nullptr),
        outerLayout(nullptr),
        searchEdit(nullptr),
        scrollArea(nullptr),
        contentWidget(nullptr),
        verticalLayout(nullptr),
        rebuildQueued(false) {
  }

  Element* root;
  Element* element;
  QVBoxLayout* outerLayout;
  QLineEdit* searchEdit;
  QScrollArea* scrollArea;
  QWidget* contentWidget;
  QVBoxLayout* verticalLayout;
  QList<AbstractParameterWidget*> parameterWidgets;
  QList<QGroupBox*> groupBoxes;
  QMap<QString, QVBoxLayout*> groupLayouts;
  QList<QWidget*> readOnlyWidgets;
  bool rebuildQueued;
};

PropertyEditorWidget::PropertyEditorWidget(Element* root, QWidget* parent)
    : QWidget(parent),
      p(std::make_unique<Private>()) {
  p->root = root;
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
}

PropertyEditorWidget::~PropertyEditorWidget() {
  clearParameterWidgets();
  clearReadOnlyWidgets();
}

void PropertyEditorWidget::initLayout() {
  if (!p->outerLayout) {
    p->outerLayout = new QVBoxLayout(this);
    p->outerLayout->setContentsMargins(0, 0, 0, 0);
    p->outerLayout->setSpacing(4);

    p->searchEdit = new QLineEdit(this);
    p->searchEdit->setObjectName(QStringLiteral("propertyEditorSearchField"));
    p->searchEdit->setPlaceholderText(tr("Search properties"));
    p->searchEdit->setClearButtonEnabled(true);
    p->searchEdit->setMinimumWidth(0);
    p->searchEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    p->outerLayout->addWidget(p->searchEdit);
    connect(p->searchEdit, &QLineEdit::textChanged, this, [this] { applyFilter(); });

    p->scrollArea = new QScrollArea(this);
    p->scrollArea->setObjectName(QStringLiteral("propertyEditorScrollArea"));
    p->scrollArea->setFrameShape(QFrame::NoFrame);
    p->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    p->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    p->scrollArea->setWidgetResizable(true);
    p->scrollArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    p->outerLayout->addWidget(p->scrollArea);
  }

  delete p->scrollArea->takeWidget();
  p->contentWidget = new QWidget(p->scrollArea);
  p->contentWidget->setObjectName(QStringLiteral("propertyEditorContent"));
  p->contentWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  p->verticalLayout = new QVBoxLayout(p->contentWidget);
  p->verticalLayout->setContentsMargins(4, 4, 4, 4);
  p->verticalLayout->setSpacing(4);
  p->scrollArea->setWidget(p->contentWidget);
}

QSize PropertyEditorWidget::sizeHint() const {
  return QSize(180, 100);
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

  auto* titleLabel = new QLabel(title, p->contentWidget);
  titleLabel->setObjectName("propertyEditorReadOnlyTitle");
  titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  QFont font = titleLabel->font();
  font.setBold(true);
  titleLabel->setFont(font);

  auto* tree = new QTreeWidget(p->contentWidget);
  tree->setObjectName("propertyEditorReadOnlyProperties");
  tree->setRootIsDecorated(false);
  tree->setAlternatingRowColors(true);
  tree->setHeaderLabels({tr("Property"), tr("Value")});
  tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  tree->header()->setStretchLastSection(true);
  tree->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

  for (const auto& row : rows) {
    auto* item = new QTreeWidgetItem(tree);
    item->setText(0, row.first);
    item->setText(1, row.second.isEmpty() ? QStringLiteral("-") : row.second);
  }

  p->readOnlyWidgets << titleLabel << tree;
  p->verticalLayout->addWidget(titleLabel);
  p->verticalLayout->addWidget(tree);
  addContentStretch();
  applyFilter();
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
  if (!p->element)
    return;

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
    if (!p->element->isPropertyVisible(name))
      continue;
    addParameter(name);
  }

  if (auto* group = qobject_cast<Group*>(p->element)) {
    auto* tree = new QTreeWidget(p->contentWidget);
    tree->setObjectName("propertyEditorGroupMetadata");
    tree->setRootIsDecorated(false);
    tree->setAlternatingRowColors(true);
    tree->setHeaderLabels({tr("Metadata"), tr("Value")});
    tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tree->header()->setStretchLastSection(true);
    tree->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    const auto metadata = group->metadata();
    if (metadata.isEmpty()) {
      auto* item = new QTreeWidgetItem(tree);
      item->setText(0, tr("metadata"));
      item->setText(1, QStringLiteral("-"));
    } else {
      for (auto i = metadata.begin(); i != metadata.end(); ++i) {
        auto* item = new QTreeWidgetItem(tree);
        item->setText(0, i.key());
        item->setText(1, metadataValueText(i.value()));
      }
    }
    p->readOnlyWidgets << tree;
    p->verticalLayout->addWidget(tree);
  }

  addContentStretch();
  applyFilter();
  p->contentWidget->adjustSize();
}

void PropertyEditorWidget::addParametersForClass(const QMetaObject* klass) {
  if (klass->className() != QString("Element") && klass->superClass()) {
    addParametersForClass(klass->superClass());
  }

  for (int i = klass->propertyOffset(); i != klass->propertyCount(); ++i) {
    auto metaProp = klass->property(i);
    if (metaProp.name() == QString("id"))
      continue;
    if (!p->element->isPropertyVisible(metaProp.name()))
      continue;

    addParameter(metaProp.name());
  }
}

void PropertyEditorWidget::addParameter(const QString& name) {
  auto value = p->element->property(name.toStdString().c_str());
  auto type = QString(value.typeName());

  AbstractParameterWidget* widget = nullptr;
  const QStringList choices = p->element->propertyChoices(name);
  const QList<int> intChoices = p->element->propertyIntChoices(name);

  if (!intChoices.isEmpty() && type == "int") {
    QVariantList choiceValues;
    choiceValues.reserve(intChoices.size());
    for (int choice : intChoices) {
      choiceValues << choice;
    }
    widget = new ChoiceParameterWidget(choiceValues, this);
  } else if (!choices.isEmpty() && type == "QString") {
    widget = new ChoiceParameterWidget(choices, this);
  } else if (isType(type, "Vector3d", "Vector3<double>")) {
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
  layoutForGroup(p->element->propertyGroup(widget->parameterName()))->addWidget(widget);
  widget->setElement(p->element);

  connect(widget, SIGNAL(changed(const QString&, const QVariant&)), this,
          SLOT(elementChanged(const QString&, const QVariant&)));
}

void PropertyEditorWidget::clearParameterWidgets() {
  qDeleteAll(p->groupBoxes);
  p->groupBoxes.clear();
  p->groupLayouts.clear();
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
  p->element->propertyEdited(propertyName);
  emit changed(p->element);
  if (p->element->rebuildPropertyEditorAfterChange(propertyName)) {
    rebuildEditorLater();
  }
}

void PropertyEditorWidget::rebuildEditorLater() {
  if (p->rebuildQueued)
    return;

  p->rebuildQueued = true;
  QTimer::singleShot(0, this, [this] {
    p->rebuildQueued = false;
    if (p->element)
      setElement(p->element);
  });
}

QVBoxLayout* PropertyEditorWidget::layoutForGroup(const QString& groupName) {
  const QString title = groupName.trimmed().isEmpty() ? QStringLiteral("Properties") : groupName;
  if (p->groupLayouts.contains(title))
    return p->groupLayouts.value(title);

  auto* groupBox = new QGroupBox(title, p->contentWidget);
  groupBox->setObjectName(QStringLiteral("propertyGroup%1").arg(title.simplified().remove(' ')));
  groupBox->setCheckable(true);
  groupBox->setChecked(true);
  groupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  connect(groupBox, &QGroupBox::toggled, this, [this] { applyFilter(); });

  auto* layout = new QVBoxLayout(groupBox);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(2);

  p->groupBoxes << groupBox;
  p->groupLayouts.insert(title, layout);
  p->verticalLayout->addWidget(groupBox);
  return layout;
}

void PropertyEditorWidget::addContentStretch() {
  if (p->verticalLayout)
    p->verticalLayout->addStretch(1);
}

void PropertyEditorWidget::applyFilter() {
  const bool hasFilter = !currentFilterText().isEmpty();

  for (auto* groupBox : p->groupBoxes) {
    bool groupHasMatch = false;
    for (auto* widget : p->parameterWidgets) {
      if (widget->parentWidget() != groupBox)
        continue;

      const bool matches = parameterMatchesFilter(widget);
      groupHasMatch = groupHasMatch || matches;
      widget->setVisible(matches && groupBox->isChecked());
    }
    groupBox->setVisible(!hasFilter || groupHasMatch);
  }

  for (auto* widget : p->readOnlyWidgets) {
    if (auto* tree = qobject_cast<QTreeWidget*>(widget)) {
      bool treeHasMatch = false;
      for (int i = 0; i != tree->topLevelItemCount(); ++i) {
        auto* item = tree->topLevelItem(i);
        const bool matches = readOnlyItemMatchesFilter(item);
        item->setHidden(!matches);
        treeHasMatch = treeHasMatch || matches;
      }
      tree->setVisible(!hasFilter || treeHasMatch);
    } else {
      widget->setVisible(true);
    }
  }
}

bool PropertyEditorWidget::parameterMatchesFilter(const AbstractParameterWidget* widget) const {
  const QString filter = currentFilterText();
  if (filter.isEmpty() || !p->element)
    return true;

  const QString parameterName = widget->parameterName();
  return parameterName.contains(filter, Qt::CaseInsensitive) ||
         p->element->propertyDisplayName(parameterName).contains(filter, Qt::CaseInsensitive) ||
         p->element->propertyDescription(parameterName).contains(filter, Qt::CaseInsensitive) ||
         p->element->propertyGroup(parameterName).contains(filter, Qt::CaseInsensitive);
}

bool PropertyEditorWidget::readOnlyItemMatchesFilter(const QTreeWidgetItem* item) const {
  const QString filter = currentFilterText();
  if (filter.isEmpty())
    return true;

  for (int column = 0; column != item->columnCount(); ++column) {
    if (item->text(column).contains(filter, Qt::CaseInsensitive))
      return true;
  }
  return false;
}

QString PropertyEditorWidget::currentFilterText() const {
  return p->searchEdit ? p->searchEdit->text().trimmed() : QString();
}

bool PropertyEditorWidget::isType(const QString& actual, const char* qt5Name,
                                  const char* qt6Name) const {
  return actual == qt5Name || actual == qt6Name;
}
