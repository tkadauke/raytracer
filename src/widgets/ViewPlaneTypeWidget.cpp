#include "widgets/ViewPlaneTypeWidget.h"
#include "ViewPlaneTypeWidget.uic"
#include "ViewPlaneFactory.h"

#include <list>

using namespace std;

struct ViewPlaneTypeWidget::Private {
  Ui::ViewPlaneTypeWidget ui;
};

ViewPlaneTypeWidget::ViewPlaneTypeWidget(QWidget* parent)
  : QWidget(parent, Qt::Drawer), p(new Private)
{
  p->ui.setupUi(this);
  list<string> types = ViewPlaneFactory::self().identifiers();
  for (list<string>::const_iterator i = types.begin(); i != types.end(); ++i) {
    p->ui.m_viewPlaneTypeComboBox->addItem(QString::fromStdString(*i));
  }
  connect(p->ui.m_viewPlaneTypeComboBox, SIGNAL(activated(int)), this, SLOT(typeChanged()));
}

ViewPlaneTypeWidget::~ViewPlaneTypeWidget() {
  delete p;
}

string ViewPlaneTypeWidget::type() const {
  return p->ui.m_viewPlaneTypeComboBox->currentText().toStdString();
}

void ViewPlaneTypeWidget::typeChanged() {
  emit changed();
}

#include "ViewPlaneTypeWidget.moc"
