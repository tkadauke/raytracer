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
#include <QFileDialog>

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
#include "world/objects/PhongMaterial.h"

MainWindow::MainWindow()
  : QMainWindow(), m_currentElement(nullptr)
{
  m_scene = new ::Scene(nullptr);

  m_display = new Display(this);
  m_display->setScene(m_scene);
  setCentralWidget(m_display);
  
  addDockWidget(Qt::LeftDockWidgetArea, createElementSelector());
  addDockWidget(Qt::RightDockWidgetArea, createPropertyEditor());
  
  createActions();
  createMenus();
}

void MainWindow::createActions() {
  m_newAct = new QAction(tr("&New"), this);
  m_newAct->setShortcuts(QKeySequence::New);
  m_newAct->setStatusTip(tr("Create a new file"));
  connect(m_newAct, SIGNAL(triggered()), this, SLOT(newFile()));
  
  m_openAct = new QAction(tr("&Open"), this);
  m_openAct->setShortcuts(QKeySequence::Open);
  m_openAct->setStatusTip(tr("Open a file from disk"));
  connect(m_openAct, SIGNAL(triggered()), this, SLOT(openFile()));
  
  m_saveAct = new QAction(tr("&Save"), this);
  m_saveAct->setShortcuts(QKeySequence::Save);
  m_saveAct->setStatusTip(tr("Save the current project to a file"));
  connect(m_saveAct, SIGNAL(triggered()), this, SLOT(saveFile()));
  
  m_saveAsAct = new QAction(tr("Save &As"), this);
  m_saveAsAct->setShortcuts(QKeySequence::SaveAs);
  m_saveAsAct->setStatusTip(tr("Save the current project to a new file"));
  connect(m_saveAsAct, SIGNAL(triggered()), this, SLOT(saveFileAs()));
  
  m_addBoxAct = new QAction(tr("Box"), this);
  m_addBoxAct->setStatusTip(tr("Add a Box to the scene"));
  connect(m_addBoxAct, SIGNAL(triggered()), this, SLOT(addBox()));
  
  m_addSphereAct = new QAction(tr("Sphere"), this);
  m_addSphereAct->setStatusTip(tr("Add a Sphere to the scene"));
  connect(m_addSphereAct, SIGNAL(triggered()), this, SLOT(addSphere()));
  
  m_addMatteMaterialAct = new QAction(tr("Matte Material"), this);
  m_addMatteMaterialAct->setStatusTip(tr("Add a matte material to the scene"));
  connect(m_addMatteMaterialAct, SIGNAL(triggered()), this, SLOT(addMatteMaterial()));

  m_addPhongMaterialAct = new QAction(tr("Phong Material"), this);
  m_addPhongMaterialAct->setStatusTip(tr("Add a Phong material to the scene"));
  connect(m_addPhongMaterialAct, SIGNAL(triggered()), this, SLOT(addPhongMaterial()));

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
  
  auto modifyingActions = {
    m_newAct, m_openAct, m_saveAct, m_saveAsAct, m_addBoxAct, m_addSphereAct,
    m_addMatteMaterialAct, m_deleteElementAct
  };
  
  for (auto& act : modifyingActions) {
    connect(act, SIGNAL(triggered()), this, SLOT(updateWindowModified()));
  }
}

void MainWindow::createMenus() {
  m_fileMenu = menuBar()->addMenu(tr("&File"));
  m_fileMenu->addAction(m_newAct);
  m_fileMenu->addAction(m_openAct);
  m_fileMenu->addAction(m_saveAct);
  m_fileMenu->addAction(m_saveAsAct);

  m_editMenu = menuBar()->addMenu(tr("&Edit"));
  auto addPrimitive = m_editMenu->addMenu(tr("Add Primitive"));
  addPrimitive->addAction(m_addBoxAct);
  addPrimitive->addAction(m_addSphereAct);
  
  auto addMaterial = m_editMenu->addMenu(tr("Add Material"));
  addMaterial->addAction(m_addMatteMaterialAct);
  addMaterial->addAction(m_addPhongMaterialAct);
  
  m_editMenu->addSeparator();
  m_editMenu->addAction(m_deleteElementAct);

  m_helpMenu = menuBar()->addMenu(tr("&Help"));
  m_helpMenu->addAction(m_aboutAct);
  m_helpMenu->addAction(m_helpAct);
}

bool MainWindow::maybeSave() {
  if (m_scene->changed()) {
    auto response = QMessageBox::question(this, tr("Save changes?"), tr("There are unsaved changes to this document. Would you like to save them?"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    switch (response) {
      case QMessageBox::Save: {
        saveFile();
        return true;
      }
      case QMessageBox::Discard: {
        m_scene->setChanged(false);
        return true;
      }
      default: {
        return false;
      }
    }
  }
  
  return true;
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (maybeSave()) {
    event->accept();
  } else {
    event->ignore();
  }
}

void MainWindow::newFile() {
  if (maybeSave()) {
    if (m_scene)
      delete m_scene;
  
    m_fileName = QString();
    m_currentElement = nullptr;
  
    m_scene = new ::Scene(nullptr);
    m_propertyEditorWidget->setRoot(m_scene);
    m_display->setScene(m_scene);

    m_elementModel->setElement(m_scene);
  }
}

void MainWindow::openFile() {
  QString fileName = QFileDialog::getOpenFileName(
    this, tr("Open File"), QString(), tr("Scenes (*.json)"));

  if (!fileName.isNull()) {
    newFile();
    m_scene->load(fileName);
    m_fileName = fileName;
    
    redraw();
  }
}

void MainWindow::saveFile() {
  if (m_fileName.isNull()) {
    saveFileAs();
  } else {
    m_scene->save(m_fileName);
  }
}

void MainWindow::saveFileAs() {
  QString fileName = QFileDialog::getSaveFileName(
    this, tr("Save File"), m_fileName, tr("Scenes (*.json)"));
  
  if (!fileName.isNull()) {
    m_fileName = fileName;
    m_scene->save(m_fileName);
  }
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

template<class Mat>
void MainWindow::addMaterial(const QString& name) {
  auto material = new Mat(m_scene);
  material->setName(QString("%1 %2").arg(name).arg(m_scene->children().size()));

  m_elementModel->setElement(m_scene);
  m_scene->setChanged(true);
}

void MainWindow::addMatteMaterial() {
  addMaterial<MatteMaterial>("MatteMaterial");
}

void MainWindow::addPhongMaterial() {
  addMaterial<PhongMaterial>("PhongMaterial");
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

void MainWindow::elementChanged(Element*) {
  m_scene->setChanged(true);
  updateWindowModified();
  redraw();
}

void MainWindow::elementSelected(const QModelIndex& current, const QModelIndex&) {
  auto element = static_cast<Element*>(current.internalPointer());
  m_currentElement = element;
  m_deleteElementAct->setEnabled(element != nullptr);
  
  if (element) {
    m_propertyEditorWidget->setElement(element);
  }
}

void MainWindow::updateWindowModified() {
  setWindowModified(m_scene->changed());
}

void MainWindow::redraw() {
  m_display->setScene(m_scene);
}

#include "MainWindow.moc"
