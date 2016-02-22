#include "SceneWidget.h"
#include "SceneWidget.uic"
#include "SceneFactory.h"

#include <list>

using namespace std;

struct SceneWidget::Private {
  Ui::SceneWidget ui;
};

SceneWidget::SceneWidget(QWidget* parent)
  : QWidget(parent), p(std::make_unique<Private>())
{
  p->ui.setupUi(this);
  list<string> types = SceneFactory::self().identifiers();
  for (const auto& i : types) {
    p->ui.m_sceneComboBox->addItem(QString::fromStdString(i));
  }
  connect(p->ui.m_sceneComboBox, SIGNAL(activated(int)), this, SLOT(sceneChanged()));
}

SceneWidget::~SceneWidget() {
}

string SceneWidget::sceneName() const {
  return p->ui.m_sceneComboBox->currentText().toStdString();
}

void SceneWidget::sceneChanged() {
  emit changed();
}

#include "SceneWidget.moc"
