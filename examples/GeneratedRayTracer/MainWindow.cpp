#include <QVBoxLayout>
#include <QSpacerItem>
#include <QDockWidget>
#include <QTreeView>
#include <QItemSelectionModel>

#include <QMouseEvent>

#include "MainWindow.h"
#include "Display.h"
#include "raytracer/Raytracer.h"
#include "raytracer/primitives/Primitive.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/Light.h"
#include "raytracer/cameras/PinholeCamera.h"
#include "core/math/HitPointInterval.h"

#include "widgets/world/PropertyEditorWidget.h"
#include "widgets/world/ElementModel.h"

#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"
#include "world/objects/Box.h"

#include "world/objects/MatteMaterial.h"

MainWindow::MainWindow()
  : QMainWindow()
{
  m_display = new Display(this);
  setCentralWidget(m_display);
    
  initScene();
  m_display->setScene(m_scene);
  
  addDockWidget(Qt::LeftDockWidgetArea, createPropertyEditor());
  addDockWidget(Qt::RightDockWidgetArea, createElementSelector());
}

QDockWidget* MainWindow::createPropertyEditor() {
  m_propertyEditorWidget = new PropertyEditorWidget(m_scene, this);

  connect(m_propertyEditorWidget, SIGNAL(changed(Element*)), this, SLOT(elementChanged(Element*)));

  auto dockWidget = new QDockWidget("Properties", this);
  dockWidget->setWidget(m_propertyEditorWidget);
  
  return dockWidget;
}

QDockWidget* MainWindow::createElementSelector() {
  m_elementModel = new ElementModel(m_scene);
  auto elementTree = new QTreeView(this);
  elementTree->setModel(m_elementModel);
  auto itemSelectionModel = new QItemSelectionModel(m_elementModel);
  elementTree->setSelectionModel(itemSelectionModel);
  
  connect(itemSelectionModel, SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(elementSelected(const QModelIndex&, const QModelIndex&)));
  
  auto dockWidget = new QDockWidget("Elements", this);
  dockWidget->setWidget(elementTree);
  
  return dockWidget;
}

void MainWindow::initScene() {
  m_scene = new ::Scene(0);
  auto material = new MatteMaterial(m_scene);
  material->setObjectName("MatteMaterial 1");
  material->setDiffuseColor(Colord(1, 0, 0));

  auto sphere = new Sphere(m_scene);
  sphere->setObjectName("Sphere 1");
  sphere->setRadius(1);
  sphere->setMaterial(material);

  sphere = new Sphere(m_scene);
  sphere->setRadius(1);
  sphere->setObjectName("Sphere 2");
  sphere->setPosition(Vector3d(1, 0, 0));
  sphere->setMaterial(material);
  
  auto box = new Box(m_scene);
  box->setPosition(Vector3d(0, 2, 0));
  box->setSize(Vector3d(1, 1, 1));
  box->setObjectName("Box 1");
  box->setMaterial(material);
}

void MainWindow::elementChanged(Element*) {
  m_display->setScene(m_scene);
}

void MainWindow::elementSelected(const QModelIndex& current, const QModelIndex&) {
  auto element = static_cast<Element*>(current.internalPointer());
  if (element) {
    m_propertyEditorWidget->setElement(element);
  }
}

#include "MainWindow.moc"
