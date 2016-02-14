#include "widgets/world/ReferenceParameterWidget.h"
#include "ReferenceParameterWidget.uic"

#include "world/objects/Element.h"

struct ReferenceParameterWidget::Private {
  Ui::ReferenceParameterWidget ui;
  
  QString baseClassName;
  Element* root;
};

ReferenceParameterWidget::ReferenceParameterWidget(const QString& baseClassName, Element* root, QWidget* parent)
  : AbstractParameterWidget(parent), p(new Private)
{
  p->baseClassName = baseClassName;
  p->root = root;
  
  p->ui.setupUi(this);
  connect(p->ui.m_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(parameterChanged()));

  fillComboBox(p->root);
}

ReferenceParameterWidget::~ReferenceParameterWidget() {
  delete p;
}

void ReferenceParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->ui.m_label->setText(name);
}

const QVariant ReferenceParameterWidget::value() {
  auto index = p->ui.m_comboBox->currentIndex();
  return p->ui.m_comboBox->itemData(index);
}

void ReferenceParameterWidget::setValue(const QVariant& value) {
  auto index = p->ui.m_comboBox->findData(value);
  p->ui.m_comboBox->setCurrentIndex(index);
}

void ReferenceParameterWidget::fillComboBox(Element* root) {
  if (root->inherits(p->baseClassName.toStdString().c_str())) {
    p->ui.m_comboBox->addItem(root->name(), QVariant::fromValue<QObject*>(root));
  }

  for (const auto& child : root->children()) {
    fillComboBox(static_cast<Element*>(child));
  }
}

#include "ReferenceParameterWidget.moc"
