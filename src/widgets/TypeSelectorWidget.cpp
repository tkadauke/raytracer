#include "widgets/TypeSelectorWidget.h"

#include <QComboBox>
#include <QString>

using namespace std;

TypeSelectorWidget::TypeSelectorWidget(QWidget* parent)
    : QWidget(parent) {
}

void TypeSelectorWidget::typeChanged() {
  emit changed();
}

void TypeSelectorWidget::populateComboBox(QComboBox* comboBox,
                                          const list<string>& identifiers) {
  for (const auto& type : identifiers) {
    comboBox->addItem(QString::fromStdString(type));
  }
  connect(comboBox, SIGNAL(activated(int)), this, SLOT(typeChanged()));
}
