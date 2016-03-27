#include "widgets/world/ReferenceParameterWidget.h"
#include "ReferenceParameterWidget.uic"

#include "world/objects/Element.h"
#include "core/Exception.h"

#include "world/objects/Material.h"
#include "world/objects/Texture.h"

Q_DECLARE_METATYPE(Material*);
Q_DECLARE_METATYPE(Texture*);

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

  p->ui.comboBox->addItem(tr("<No Selection>"), makeVariant(nullptr));
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
    p->ui.comboBox->addItem(root->name(), makeVariant(root));
  }

  for (const auto& child : root->childElements()) {
    fillComboBox(child);
  }
}

QVariant ReferenceParameterWidget::makeVariant(Element* e) {
  if (p->baseClassName == "Material") {
    return QVariant::fromValue<Material*>(static_cast<Material*>(e));
  } else if (p->baseClassName == "Texture") {
    return QVariant::fromValue<Texture*>(static_cast<Texture*>(e));
  } else {
    throw Exception(("Unknown reference type " + p->baseClassName).toStdString(), __FILE__, __LINE__);
  }
}

#include "ReferenceParameterWidget.moc"
