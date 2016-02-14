#include <QVBoxLayout>
#include <QSpacerItem>
#include <QDockWidget>
#include <QTreeView>
#include <QItemSelectionModel>

#include <QMouseEvent>

#include <QAction>
#include <QMenuBar>
#include <QMessageBox>
#include <QDesktopServices>

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
  : QMainWindow(), m_currentElement(nullptr)
{
  initScene();
  
  m_display = new Display(this);
  m_display->setScene(m_scene);
  setCentralWidget(m_display);
  
  addDockWidget(Qt::LeftDockWidgetArea, createPropertyEditor());
  addDockWidget(Qt::RightDockWidgetArea, createElementSelector());
  
  createActions();
  createMenus();
}

void MainWindow::createActions() {
  m_newAct = new QAction(tr("&New"), this);
  m_newAct->setShortcuts(QKeySequence::New);
  m_newAct->setStatusTip(tr("Create a new file"));
  connect(m_newAct, SIGNAL(triggered()), this, SLOT(newFile()));
  
  m_addBoxAct = new QAction(tr("Box"), this);
  m_addBoxAct->setStatusTip(tr("Add a Box to the scene"));
  connect(m_addBoxAct, SIGNAL(triggered()), this, SLOT(addBox()));
  
  m_addSphereAct = new QAction(tr("Sphere"), this);
  m_addSphereAct->setStatusTip(tr("Add a Sphere to the scene"));
  connect(m_addSphereAct, SIGNAL(triggered()), this, SLOT(addSphere()));
  
  m_addMatteMaterialAct = new QAction(tr("Matte Material"), this);
  m_addMatteMaterialAct->setStatusTip(tr("Add a matte material to the scene"));
  connect(m_addMatteMaterialAct, SIGNAL(triggered()), this, SLOT(addMatteMaterial()));

  m_aboutAct = new QAction(tr("&About"), this);
  m_aboutAct->setStatusTip(tr("Show the application's About box"));
  connect(m_aboutAct, SIGNAL(triggered()), this, SLOT(about()));

  m_deleteElementAct = new QAction(tr("&Delete"), this);
  m_deleteElementAct->setShortcuts(QKeySequence::Delete);
  m_deleteElementAct->setStatusTip(tr("Delete selected element"));
  m_deleteElementAct->setEnabled(false);
  connect(m_deleteElementAct, SIGNAL(triggered()), this, SLOT(deleteElement()));

  m_helpAct = new QAction(tr("Raytracer &Help"), this);
  m_helpAct->setStatusTip(tr("Go to the Github page"));
  connect(m_helpAct, SIGNAL(triggered()), this, SLOT(help()));
}

void MainWindow::createMenus() {
  m_fileMenu = menuBar()->addMenu(tr("&File"));
  m_fileMenu->addAction(m_newAct);

  m_editMenu = menuBar()->addMenu(tr("&Edit"));
  auto addPrimitive = m_editMenu->addMenu(tr("Add Primitive"));
  addPrimitive->addAction(m_addBoxAct);
  addPrimitive->addAction(m_addSphereAct);
  
  auto addMaterial = m_editMenu->addMenu(tr("Add Material"));
  addMaterial->addAction(m_addMatteMaterialAct);
  
  m_editMenu->addSeparator();
  m_editMenu->addAction(m_deleteElementAct);

  m_helpMenu = menuBar()->addMenu(tr("&Help"));
  m_helpMenu->addAction(m_aboutAct);
  m_helpMenu->addAction(m_helpAct);
}

void MainWindow::newFile() {
  if (m_scene)
    delete m_scene;
  
  m_currentElement = nullptr;
  
  m_scene = new ::Scene(nullptr);
  m_propertyEditorWidget->setRoot(m_scene);
  m_display->setScene(m_scene);

  m_elementModel->setElement(m_scene);
}

void MainWindow::addBox() {
  auto box = new Box(m_scene);
  box->setName(QString("Box %1").arg(m_scene->children().size()));

  m_elementModel->setElement(m_scene);
  elementChanged(box);
}

void MainWindow::addSphere() {
  auto sphere = new Sphere(m_scene);
  sphere->setName(QString("Sphere %1").arg(m_scene->children().size()));

  m_elementModel->setElement(m_scene);
  elementChanged(sphere);
}

void MainWindow::addMatteMaterial() {
  auto material = new MatteMaterial(m_scene);
  material->setName(QString("MatteMaterial %1").arg(m_scene->children().size()));

  m_elementModel->setElement(m_scene);
}

void MainWindow::deleteElement() {
  delete m_currentElement;
  m_currentElement = nullptr;

  m_elementModel->setElement(m_scene);
  m_propertyEditorWidget->setElement(nullptr);

  m_deleteElementAct->setEnabled(false);

  elementChanged(nullptr);
}

void MainWindow::about() {
  QMessageBox::about(this, tr("About"),
          tr("This is a modeler for the Raytracer library."));
}

void MainWindow::help() {
  QDesktopServices::openUrl(QUrl("https://github.com/tkadauke/raytracer"));
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
  material->setName("MatteMaterial 1");
  material->setDiffuseColor(Colord(1, 0, 0));

  auto sphere = new Sphere(m_scene);
  sphere->setName("Sphere 1");
  sphere->setMaterial(material);

  sphere = new Sphere(m_scene);
  sphere->setName("Sphere 2");
  sphere->setPosition(Vector3d(1, 0, 0));
  sphere->setMaterial(material);
  
  auto box = new Box(m_scene);
  box->setPosition(Vector3d(0, 2, 0));
  box->setName("Box 1");
  box->setMaterial(material);
}

void MainWindow::elementChanged(Element*) {
  m_display->setScene(m_scene);
}

void MainWindow::elementSelected(const QModelIndex& current, const QModelIndex&) {
  auto element = static_cast<Element*>(current.internalPointer());
  m_currentElement = element;
  m_deleteElementAct->setEnabled(element != nullptr);
  
  if (element) {
    m_propertyEditorWidget->setElement(element);
  }
}

#include "MainWindow.moc"
