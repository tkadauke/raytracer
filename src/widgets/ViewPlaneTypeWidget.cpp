#include "widgets/ViewPlaneTypeWidget.h"
#include "ui_ViewPlaneTypeWidget.h"
#include "render/viewplanes/ViewPlaneFactory.h"

using namespace std;
using namespace render;

struct ViewPlaneTypeWidget::Private {
  Ui::ViewPlaneTypeWidget ui;
};

ViewPlaneTypeWidget::ViewPlaneTypeWidget(QWidget* parent)
    : TypeSelectorWidget(parent),
      p(std::make_unique<Private>()) {
  p->ui.setupUi(this);
  populateComboBox(p->ui.viewPlaneTypeComboBox, render::ViewPlaneFactory::self().identifiers());
}

ViewPlaneTypeWidget::~ViewPlaneTypeWidget() {
}

string ViewPlaneTypeWidget::type() const {
  return p->ui.viewPlaneTypeComboBox->currentText().toStdString();
}
