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
#include "raytracer/lights/PointLight.h"
#include "raytracer/cameras/PinholeCamera.h"
#include "core/math/HitPointInterval.h"

#include "widgets/world/PropertyEditorWidget.h"
#include "widgets/world/PreviewDisplayWidget.h"
#include "widgets/world/SceneModel.h"
#include "widgets/world/RenderWindow.h"

#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"
#include "world/objects/Box.h"

#include "world/objects/Intersection.h"

#include "world/objects/MatteMaterial.h"
#include "world/objects/PhongMaterial.h"
#include "world/objects/TransparentMaterial.h"
#include "world/objects/ReflectiveMaterial.h"

#include "world/objects/ConstantColorTexture.h"
#include "world/objects/CheckerBoardTexture.h"

#include "world/objects/PinholeCamera.h"
#include "world/objects/FishEyeCamera.h"
#include "world/objects/OrthographicCamera.h"
#include "world/objects/SphericalCamera.h"

struct MainWindow::Private {
  inline Private()
    : currentElement(nullptr)
  {
  }
  
  QString fileName;

  Display* display;
  PreviewDisplayWidget* materialDisplay;
  PropertyEditorWidget* propertyEditorWidget;
  SceneModel* elementModel;
  
  RenderWindow* renderWindow;
  
  Scene* scene;
  
  Element* currentElement;
  QModelIndex currentIndex;
  
  QMenu* fileMenu;
  QMenu* editMenu;
  QMenu* renderMenu;
  QMenu* helpMenu;

  QAction* newAct;
  QAction* openAct;
  QAction* saveAct;
  QAction* saveAsAct;

  QAction* addBoxAct;
  QAction* addSphereAct;

  QAction* addIntersectionAct;
  
  QAction* addMatteMaterialAct;
  QAction* addPhongMaterialAct;
  QAction* addTransparentMaterialAct;
  QAction* addReflectiveMaterialAct;
  
  QAction* addConstantColorTextureAct;
  QAction* addCheckerBoardTextureAct;

  QAction* addPinholeCameraAct;
  QAction* addFishEyeCameraAct;
  QAction* addOrthographicCameraAct;
  QAction* addSphericalCameraAct;
  
  QAction* deleteElementAct;

  QAction* renderAct;
  
  QAction* aboutAct;
  QAction* helpAct;
};

MainWindow::MainWindow()
  : QMainWindow(), p(std::make_unique<Private>())
{
  p->scene = new ::Scene(nullptr);

  p->display = new Display(this);
  p->display->setScene(p->scene);
  setCentralWidget(p->display);
  
  addDockWidget(Qt::LeftDockWidgetArea, createElementSelector());
  addDockWidget(Qt::RightDockWidgetArea, createPropertyEditor());
  addDockWidget(Qt::RightDockWidgetArea, createPreviewDisplay());
  
  connect(this, SIGNAL(selectionChanged(Element*)), this, SLOT(updatePreviewWidget()));
  connect(this, SIGNAL(currentElementChanged()), this, SLOT(updatePreviewWidget()));
  
  createActions();
  createMenus();
  
  p->renderWindow = new RenderWindow(nullptr);
}

void MainWindow::createActions() {
  p->newAct = new QAction(tr("&New"), this);
  p->newAct->setShortcuts(QKeySequence::New);
  p->newAct->setStatusTip(tr("Create a new file"));
  connect(p->newAct, SIGNAL(triggered()), this, SLOT(newFile()));
  
  p->openAct = new QAction(tr("&Open"), this);
  p->openAct->setShortcuts(QKeySequence::Open);
  p->openAct->setStatusTip(tr("Open a file from disk"));
  connect(p->openAct, SIGNAL(triggered()), this, SLOT(openFile()));
  
  p->saveAct = new QAction(tr("&Save"), this);
  p->saveAct->setShortcuts(QKeySequence::Save);
  p->saveAct->setStatusTip(tr("Save the current project to a file"));
  connect(p->saveAct, SIGNAL(triggered()), this, SLOT(saveFile()));
  
  p->saveAsAct = new QAction(tr("Save &As"), this);
  p->saveAsAct->setShortcuts(QKeySequence::SaveAs);
  p->saveAsAct->setStatusTip(tr("Save the current project to a new file"));
  connect(p->saveAsAct, SIGNAL(triggered()), this, SLOT(saveFileAs()));
  
  p->addBoxAct = new QAction(tr("Box"), this);
  p->addBoxAct->setStatusTip(tr("Add a Box to the scene"));
  connect(p->addBoxAct, SIGNAL(triggered()), this, SLOT(addBox()));
  
  p->addSphereAct = new QAction(tr("Sphere"), this);
  p->addSphereAct->setStatusTip(tr("Add a Sphere to the scene"));
  connect(p->addSphereAct, SIGNAL(triggered()), this, SLOT(addSphere()));
  
  p->addIntersectionAct = new QAction(tr("Intersection"), this);
  p->addIntersectionAct->setStatusTip(tr("Add an intersection to the scene"));
  connect(p->addIntersectionAct, SIGNAL(triggered()), this, SLOT(addIntersection()));
  
  p->addMatteMaterialAct = new QAction(tr("Matte Material"), this);
  p->addMatteMaterialAct->setStatusTip(tr("Add a matte material to the scene"));
  connect(p->addMatteMaterialAct, SIGNAL(triggered()), this, SLOT(addMatteMaterial()));

  p->addPhongMaterialAct = new QAction(tr("Phong Material"), this);
  p->addPhongMaterialAct->setStatusTip(tr("Add a Phong material to the scene"));
  connect(p->addPhongMaterialAct, SIGNAL(triggered()), this, SLOT(addPhongMaterial()));

  p->addTransparentMaterialAct = new QAction(tr("Transparent Material"), this);
  p->addTransparentMaterialAct->setStatusTip(tr("Add a transparent material to the scene"));
  connect(p->addTransparentMaterialAct, SIGNAL(triggered()), this, SLOT(addTransparentMaterial()));

  p->addReflectiveMaterialAct = new QAction(tr("Reflective Material"), this);
  p->addReflectiveMaterialAct->setStatusTip(tr("Add a reflective material to the scene"));
  connect(p->addReflectiveMaterialAct, SIGNAL(triggered()), this, SLOT(addReflectiveMaterial()));

  p->addConstantColorTextureAct = new QAction(tr("Constant Color"), this);
  p->addConstantColorTextureAct->setStatusTip(tr("Add a constant color texture to the scene"));
  connect(p->addConstantColorTextureAct, SIGNAL(triggered()), this, SLOT(addConstantColorTexture()));

  p->addCheckerBoardTextureAct = new QAction(tr("Checker Board"), this);
  p->addCheckerBoardTextureAct->setStatusTip(tr("Add a checker board texture to the scene"));
  connect(p->addCheckerBoardTextureAct, SIGNAL(triggered()), this, SLOT(addCheckerBoardTexture()));

  p->addPinholeCameraAct = new QAction(tr("Pinhole Camera"), this);
  p->addPinholeCameraAct->setStatusTip(tr("Add a pinhole camera to the scene"));
  connect(p->addPinholeCameraAct, SIGNAL(triggered()), this, SLOT(addPinholeCamera()));

  p->addFishEyeCameraAct = new QAction(tr("Fish Eye Camera"), this);
  p->addFishEyeCameraAct->setStatusTip(tr("Add a fish eye camera to the scene"));
  connect(p->addFishEyeCameraAct, SIGNAL(triggered()), this, SLOT(addFishEyeCamera()));

  p->addOrthographicCameraAct = new QAction(tr("Orthographic Camera"), this);
  p->addOrthographicCameraAct->setStatusTip(tr("Add an orthographic camera to the scene"));
  connect(p->addOrthographicCameraAct, SIGNAL(triggered()), this, SLOT(addOrthographicCamera()));

  p->addSphericalCameraAct = new QAction(tr("Spherical Camera"), this);
  p->addSphericalCameraAct->setStatusTip(tr("Add a spherical camera to the scene"));
  connect(p->addSphericalCameraAct, SIGNAL(triggered()), this, SLOT(addSphericalCamera()));

  p->aboutAct = new QAction(tr("&About"), this);
  p->aboutAct->setStatusTip(tr("Show the application's About box"));
  connect(p->aboutAct, SIGNAL(triggered()), this, SLOT(about()));

  p->deleteElementAct = new QAction(tr("&Delete"), this);
  p->deleteElementAct->setShortcuts(QKeySequence::Delete);
  p->deleteElementAct->setStatusTip(tr("Delete selected element"));
  p->deleteElementAct->setEnabled(false);
  connect(p->deleteElementAct, SIGNAL(triggered()), this, SLOT(deleteElement()));

  p->renderAct = new QAction(tr("&Render"), this);
  p->renderAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
  p->renderAct->setStatusTip(tr("Render current scene"));
  connect(p->renderAct, SIGNAL(triggered()), this, SLOT(render()));

  p->helpAct = new QAction(tr("Raytracer &Help"), this);
  p->helpAct->setStatusTip(tr("Go to the Github page"));
  connect(p->helpAct, SIGNAL(triggered()), this, SLOT(help()));
  
  auto modifyingActions = {
    p->newAct, p->openAct, p->saveAct, p->saveAsAct, p->addBoxAct, p->addSphereAct,
    p->addIntersectionAct, p->addMatteMaterialAct, p->addPhongMaterialAct,
    p->addReflectiveMaterialAct, p->addConstantColorTextureAct,
    p->addCheckerBoardTextureAct, p->addPinholeCameraAct, p->deleteElementAct
  };
  
  for (auto& act : modifyingActions) {
    connect(act, SIGNAL(triggered()), this, SLOT(updateWindowModified()));
  }
}

void MainWindow::createMenus() {
  p->fileMenu = menuBar()->addMenu(tr("&File"));
  p->fileMenu->addAction(p->newAct);
  p->fileMenu->addAction(p->openAct);
  p->fileMenu->addAction(p->saveAct);
  p->fileMenu->addAction(p->saveAsAct);

  p->editMenu = menuBar()->addMenu(tr("&Edit"));
  auto addPrimitive = p->editMenu->addMenu(tr("Add Primitive"));
  addPrimitive->addAction(p->addBoxAct);
  addPrimitive->addAction(p->addSphereAct);

  auto addComposite = p->editMenu->addMenu(tr("Add Composite"));
  addComposite->addAction(p->addIntersectionAct);
  
  auto addMaterial = p->editMenu->addMenu(tr("Add Material"));
  addMaterial->addAction(p->addMatteMaterialAct);
  addMaterial->addAction(p->addPhongMaterialAct);
  addMaterial->addAction(p->addTransparentMaterialAct);
  addMaterial->addAction(p->addReflectiveMaterialAct);
  
  auto addTexture = p->editMenu->addMenu(tr("Add Texture"));
  addTexture->addAction(p->addConstantColorTextureAct);
  addTexture->addAction(p->addCheckerBoardTextureAct);

  auto addCamera = p->editMenu->addMenu(tr("Add Camera"));
  addCamera->addAction(p->addPinholeCameraAct);
  addCamera->addAction(p->addFishEyeCameraAct);
  addCamera->addAction(p->addOrthographicCameraAct);
  addCamera->addAction(p->addSphericalCameraAct);
  
  p->editMenu->addSeparator();
  p->editMenu->addAction(p->deleteElementAct);

  p->renderMenu = menuBar()->addMenu(tr("&Render"));
  p->renderMenu->addAction(p->renderAct);

  p->helpMenu = menuBar()->addMenu(tr("&Help"));
  p->helpMenu->addAction(p->aboutAct);
  p->helpMenu->addAction(p->helpAct);
}

bool MainWindow::maybeSave() {
  if (p->scene->changed()) {
    auto response = QMessageBox::question(
      this,
      tr("Save changes?"),
      tr("There are unsaved changes to this document. Would you like to save them?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save
    );
    
    switch (response) {
      case QMessageBox::Save: {
        saveFile();
        return true;
      }
      case QMessageBox::Discard: {
        p->scene->setChanged(false);
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
    if (p->scene)
      delete p->scene;
  
    p->fileName = QString();
    p->currentElement = nullptr;
    emit selectionChanged(nullptr);
  
    p->scene = new ::Scene(nullptr);
    p->propertyEditorWidget->setRoot(p->scene);
    p->display->setScene(p->scene);

    p->elementModel->setElement(p->scene);
  }
}

void MainWindow::openFile() {
  QString fileName = QFileDialog::getOpenFileName(
    this, tr("Open File"), QString(), tr("Scenes (*.json)"));

  if (!fileName.isNull()) {
    newFile();
    p->scene->load(fileName);
    p->fileName = fileName;
    
    redraw();
  }
}

void MainWindow::saveFile() {
  if (p->fileName.isNull()) {
    saveFileAs();
  } else {
    p->scene->save(p->fileName);
  }
}

void MainWindow::saveFileAs() {
  QString fileName = QFileDialog::getSaveFileName(
    this, tr("Save File"), p->fileName, tr("Scenes (*.json)"));
  
  if (!fileName.isNull()) {
    p->fileName = fileName;
    p->scene->save(p->fileName);
  }
}

template<class T>
void MainWindow::add() {
  auto element = new T(nullptr);
  element->setName(QString("%1 %2")
    .arg(element->metaObject()->className())
    .arg(p->scene->childElements().size()));

  p->elementModel->addElement(p->currentIndex, element);
  elementChanged(element);
}

void MainWindow::addBox() {
  add<Box>();
}

void MainWindow::addSphere() {
  add<Sphere>();
}

void MainWindow::addIntersection() {
  add<Intersection>();
}

void MainWindow::addMatteMaterial() {
  add<MatteMaterial>();
}

void MainWindow::addPhongMaterial() {
  add<PhongMaterial>();
}

void MainWindow::addTransparentMaterial() {
  add<TransparentMaterial>();
}

void MainWindow::addReflectiveMaterial() {
  add<ReflectiveMaterial>();
}

void MainWindow::addConstantColorTexture() {
  add<ConstantColorTexture>();
}

void MainWindow::addCheckerBoardTexture() {
  add<CheckerBoardTexture>();
}

void MainWindow::addPinholeCamera() {
  add<PinholeCamera>();
}

void MainWindow::addFishEyeCamera() {
  add<FishEyeCamera>();
}

void MainWindow::addOrthographicCamera() {
  add<OrthographicCamera>();
}

void MainWindow::addSphericalCamera() {
  add<SphericalCamera>();
}

void MainWindow::deleteElement() {
  p->elementModel->deleteElement(p->currentIndex);
  p->currentElement = nullptr;
  p->currentIndex = QModelIndex();
  emit selectionChanged(nullptr);

  p->propertyEditorWidget->setElement(nullptr);

  p->deleteElementAct->setEnabled(false);

  elementChanged(nullptr);
}

void MainWindow::render() {
  if (!p->renderWindow->isBusy()) {
    p->renderWindow->setScene(p->scene);
  }
  
  p->renderWindow->show();
}

void MainWindow::about() {
  QMessageBox::about(this, tr("About"),
          tr("This is a modeler for the Raytracer library."));
}

void MainWindow::help() {
  QDesktopServices::openUrl(QUrl("https://github.com/tkadauke/raytracer"));
}

QDockWidget* MainWindow::createPropertyEditor() {
  p->propertyEditorWidget = new PropertyEditorWidget(p->scene, this);

  connect(
    p->propertyEditorWidget, SIGNAL(changed(Element*)),
    this, SLOT(elementChanged(Element*))
  );

  auto dockWidget = new QDockWidget("Properties", this);
  dockWidget->setWidget(p->propertyEditorWidget);
  
  return dockWidget;
}

QDockWidget* MainWindow::createElementSelector() {
  p->elementModel = new SceneModel(p->scene);
  auto elementTree = new QTreeView(this);
  // elementTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  elementTree->setDragEnabled(true);
  elementTree->setAcceptDrops(true);
  elementTree->setDropIndicatorShown(true);
  elementTree->setModel(p->elementModel);
  auto itemSelectionModel = new QItemSelectionModel(p->elementModel);
  elementTree->setSelectionModel(itemSelectionModel);
  
  connect(
    itemSelectionModel, SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)),
    this, SLOT(elementSelected(const QModelIndex&, const QModelIndex&))
  );
    
  connect(
    p->elementModel, SIGNAL(rowsMoved(const QModelIndex&, int, int, const QModelIndex&, int)),
    this, SLOT(reorder())
  );
  
  auto dockWidget = new QDockWidget("Elements", this);
  dockWidget->setWidget(elementTree);
  
  return dockWidget;
}

QDockWidget* MainWindow::createPreviewDisplay() {
  p->materialDisplay = new PreviewDisplayWidget(this);
  
  auto dockWidget = new QDockWidget("Preview", this);
  dockWidget->setWidget(p->materialDisplay);
  
  return dockWidget;
}

void MainWindow::elementChanged(Element*) {
  p->scene->setChanged(true);
  updateWindowModified();
  redraw();
  emit currentElementChanged();
}

void MainWindow::elementSelected(const QModelIndex& current, const QModelIndex&) {
  auto element = static_cast<Element*>(current.internalPointer());
  p->currentElement = element;
  p->currentIndex = current;
  p->deleteElementAct->setEnabled(element != nullptr);
  
  if (element) {
    p->propertyEditorWidget->setElement(element);
  }
  emit selectionChanged(element);
}

void MainWindow::updateWindowModified() {
  setWindowModified(p->scene->changed());
}

void MainWindow::updatePreviewWidget() {
  Material* mat = qobject_cast<Material*>(p->currentElement);
  Camera* cam = qobject_cast<Camera*>(p->currentElement);
  if (mat) {
    p->materialDisplay->setMaterial(mat, p->scene);
  } else if (cam) {
    p->materialDisplay->setCamera(cam, p->scene);
  } else {
    p->materialDisplay->clear();
  }
}

void MainWindow::reorder() {
  redraw();
  p->scene->setChanged(true);
  updateWindowModified();
}

void MainWindow::redraw() {
  p->display->setScene(p->scene);
}

#include "MainWindow.moc"
