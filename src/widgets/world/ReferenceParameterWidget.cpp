#include "widgets/world/ReferenceParameterWidget.h"
#include "ReferenceParameterWidget.uic"

#include "world/objects/Element.h"

struct ReferenceParameterWidget::Private {
  Ui::ReferenceParameterWidget ui;
  
  QString baseClassName;
  Element* root;
};

ReferenceParameterWidget::ReferenceParameterWidget(const QString& baseClassName, Element* root, QWidget* parent)
  : AbstractParameterWidget(parent),
    p(std::make_unique<Private>())
{
  p->baseClassName = baseClassName;
  p->root = root;
  
  p->ui.setupUi(this);
  connect(p->ui.comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(parameterChanged()));

  p->ui.comboBox->addItem(tr("<No Selection>"), QVariant::fromValue<QObject*>(nullptr));
  fillComboBox(p->root);
}

ReferenceParameterWidget::~ReferenceParameterWidget() {
}

void ReferenceParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->ui.label->setText(name);
}

const QVariant ReferenceParameterWidget::value() const {
  auto index = p->ui.comboBox->currentIndex();
  return p->ui.comboBox->itemData(index);
}

void ReferenceParameterWidget::setValue(const QVariant& value) {
  auto index = p->ui.comboBox->findData(value);
  p->ui.comboBox->setCurrentIndex(index);
}

void ReferenceParameterWidget::fillComboBox(Element* root) {
  if (root->inherits(p->baseClassName.toStdString().c_str())) {
    p->ui.comboBox->addItem(root->name(), QVariant::fromValue<QObject*>(root));
  }

  for (const auto& child : root->childElements()) {
    fillComboBox(child);
  }
}

#include "ReferenceParameterWidget.moc"
