#include "widgets/world/ChoiceParameterWidget.h"

#include <QComboBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVariant>
#include <QVBoxLayout>

#include <utility>

struct ChoiceParameterWidget::Private {
  QLabel* label{nullptr};
  QComboBox* comboBox{nullptr};
  QVariantList choices;
};

ChoiceParameterWidget::ChoiceParameterWidget(QStringList choices, QWidget* parent)
    : ChoiceParameterWidget(QVariantList{}, parent) {
  p->choices.reserve(choices.size());
  for (const QString& choice : choices) {
    p->choices << choice;
  }
}

ChoiceParameterWidget::ChoiceParameterWidget(QVariantList choices, QWidget* parent)
    : AbstractParameterWidget(parent),
      p(std::make_unique<Private>()) {
  p->choices = std::move(choices);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(2, 2, 2, 2);
  layout->setSpacing(2);

  p->label = new QLabel(this);
  p->label->setWordWrap(true);
  p->label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  p->comboBox = new QComboBox(this);
  p->comboBox->setObjectName(QStringLiteral("choiceComboBox"));
  p->comboBox->setMinimumWidth(0);
  p->comboBox->setMinimumContentsLength(6);
  p->comboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  p->comboBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

  layout->addWidget(p->label);
  layout->addWidget(p->comboBox);

  connect(p->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ChoiceParameterWidget::parameterChanged);
}

ChoiceParameterWidget::~ChoiceParameterWidget() {
}

void ChoiceParameterWidget::setParameterName(const QString& name) {
  AbstractParameterWidget::setParameterName(name);
  populateChoices();
}

void ChoiceParameterWidget::setLabelText(const QString& text) {
  p->label->setText(text);
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
  for (const QVariant& choice : p->choices) {
    p->comboBox->addItem(displayNameForChoice(choice.toString()), choice);
  }
}
