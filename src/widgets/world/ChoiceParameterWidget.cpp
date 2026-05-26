#include "widgets/world/ChoiceParameterWidget.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>

#include <utility>

struct ChoiceParameterWidget::Private {
  QLabel* label{nullptr};
  QComboBox* comboBox{nullptr};
  QStringList choices;
};

ChoiceParameterWidget::ChoiceParameterWidget(QStringList choices, QWidget* parent)
    : AbstractParameterWidget(parent),
      p(std::make_unique<Private>()) {
  p->choices = std::move(choices);

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(6, 2, 6, 2);
  layout->setSpacing(8);

  p->label = new QLabel(this);
  p->label->setMinimumWidth(135);
  p->comboBox = new QComboBox(this);
  p->comboBox->setObjectName(QStringLiteral("choiceComboBox"));

  layout->addWidget(p->label);
  layout->addWidget(p->comboBox, 1);

  connect(p->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ChoiceParameterWidget::parameterChanged);
}

ChoiceParameterWidget::~ChoiceParameterWidget() {
}

void ChoiceParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  p->label->setText(displayNameForParameter(name));
  populateChoices();
}

const QVariant ChoiceParameterWidget::value() const {
  return p->comboBox->currentData().isValid() ? p->comboBox->currentData()
                                              : QVariant::fromValue(p->comboBox->currentText());
}

void ChoiceParameterWidget::setValue(const QVariant& value) {
  const QSignalBlocker blocker(p->comboBox);
  int index = p->comboBox->findData(value);
  if (index < 0) {
    index = p->comboBox->findText(value.toString());
  }
  if (index >= 0) {
    p->comboBox->setCurrentIndex(index);
  }
}

void ChoiceParameterWidget::populateChoices() {
  const QSignalBlocker blocker(p->comboBox);
  p->comboBox->clear();
  for (const QString& choice : p->choices) {
    p->comboBox->addItem(displayNameForChoice(choice), choice);
  }
}
