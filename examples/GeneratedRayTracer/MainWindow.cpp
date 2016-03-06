#include <QGuiApplication>

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
#include "world/objects/Union.h"
#include "world/objects/Difference.h"

#include "world/objects/MatteMaterial.h"
#include "world/objects/PhongMaterial.h"
#include "world/objects/TransparentMaterial.h"
#include "world/objects/ReflectiveMaterial.h"

#include "world/objects/ConstantColorTexture.h"
#include "world/objects/CheckerBoardTexture.h"

#include "world/objects/DirectionalLight.h"
#include "world/objects/PointLight.h"

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
  QAction* addUnionAct;
  QAction* addDifferenceAct;
  
  QAction* addMatteMaterialAct;
  QAction* addPhongMaterialAct;
  QAction* addTransparentMaterialAct;
  QAction* addReflectiveMaterialAct;
  
  QAction* addConstantColorTextureAct;
  QAction* addCheckerBoardTextureAct;

  QAction* addDirectionalLightAct;
  QAction* addPointLightAct;

  QAction* addPinholeCameraAct;
  QAction* addFishEyeCameraAct;
  QAction* addOrthographicCameraAct;
  QAction* addSphericalCameraAct;
  
  QAction* deleteElementAct;

  QAction* moveForwardsAlongXAct;
  QAction* moveBackwardsAlongXAct;
  QAction* moveForwardsAlongYAct;
  QAction* moveBackwardsAlongYAct;
  QAction* moveForwardsAlongZAct;
  QAction* moveBackwardsAlongZAct;

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
  
  p->addUnionAct = new QAction(tr("Union"), this);
  p->addUnionAct->setStatusTip(tr("Add a union to the scene"));
  connect(p->addUnionAct, SIGNAL(triggered()), this, SLOT(addUnion()));
  
  p->addDifferenceAct = new QAction(tr("Difference"), this);
  p->addDifferenceAct->setStatusTip(tr("Add a difference to the scene"));
  connect(p->addDifferenceAct, SIGNAL(triggered()), this, SLOT(addDifference()));
  
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

  p->addDirectionalLightAct = new QAction(tr("Directional Light"), this);
  p->addDirectionalLightAct->setStatusTip(tr("Add a directional light to the scene"));
  connect(p->addDirectionalLightAct, SIGNAL(triggered()), this, SLOT(addDirectionalLight()));

  p->addPointLightAct = new QAction(tr("Point Light"), this);
  p->addPointLightAct->setStatusTip(tr("Add a point light to the scene"));
  connect(p->addPointLightAct, SIGNAL(triggered()), this, SLOT(addPointLight()));

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

  p->moveForwardsAlongXAct = new QAction(tr("Move forwards along X axis"), this);
  p->moveForwardsAlongXAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::META + Qt::Key_Up),
    QKeySequence(Qt::SHIFT + Qt::META + Qt::Key_Up)
  });
  p->moveForwardsAlongXAct->setStatusTip(tr("Moves the current element forwards along the X axis"));
  connect(p->moveForwardsAlongXAct, SIGNAL(triggered()), this, SLOT(moveForwardsAlongX()));

  p->moveBackwardsAlongXAct = new QAction(tr("Move backwards along X axis"), this);
  p->moveBackwardsAlongXAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::META + Qt::Key_Down),
    QKeySequence(Qt::SHIFT + Qt::META + Qt::Key_Down)
  });
  p->moveBackwardsAlongXAct->setStatusTip(tr("Moves the current element backwards along the X axis"));
  connect(p->moveBackwardsAlongXAct, SIGNAL(triggered()), this, SLOT(moveBackwardsAlongX()));

  p->moveForwardsAlongYAct = new QAction(tr("Move forwards along Y axis"), this);
  p->moveForwardsAlongYAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::ALT + Qt::Key_Up),
    QKeySequence(Qt::SHIFT + Qt::ALT + Qt::Key_Up)
  });
  p->moveForwardsAlongYAct->setStatusTip(tr("Moves the current element forwards along the Y axis"));
  connect(p->moveForwardsAlongYAct, SIGNAL(triggered()), this, SLOT(moveForwardsAlongY()));

  p->moveBackwardsAlongYAct = new QAction(tr("Move backwards along Y axis"), this);
  p->moveBackwardsAlongYAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::ALT + Qt::Key_Down),
    QKeySequence(Qt::SHIFT + Qt::ALT + Qt::Key_Down)
  });
  p->moveBackwardsAlongYAct->setStatusTip(tr("Moves the current element backwards along the Y axis"));
  connect(p->moveBackwardsAlongYAct, SIGNAL(triggered()), this, SLOT(moveBackwardsAlongY()));

  p->moveForwardsAlongZAct = new QAction(tr("Move forwards along Z axis"), this);
  p->moveForwardsAlongZAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::CTRL + Qt::Key_Up),
    QKeySequence(Qt::SHIFT + Qt::CTRL + Qt::Key_Up)
  });
  p->moveForwardsAlongZAct->setStatusTip(tr("Moves the current element forwards along the Z axis"));
  connect(p->moveForwardsAlongZAct, SIGNAL(triggered()), this, SLOT(moveForwardsAlongZ()));

  p->moveBackwardsAlongZAct = new QAction(tr("Move backwards along Z axis"), this);
  p->moveBackwardsAlongZAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::CTRL + Qt::Key_Down),
    QKeySequence(Qt::SHIFT + Qt::CTRL + Qt::Key_Down)
  });
  p->moveBackwardsAlongZAct->setStatusTip(tr("Moves the current element backwards along the Z axis"));
  connect(p->moveBackwardsAlongZAct, SIGNAL(triggered()), this, SLOT(moveBackwardsAlongZ()));

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
    p->newAct, p->openAct, p->saveAct, p->saveAsAct, p->addBoxAct, p->deleteElementAct
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
  addComposite->addAction(p->addUnionAct);
  addComposite->addAction(p->addDifferenceAct);
  
  auto addMaterial = p->editMenu->addMenu(tr("Add Material"));
  addMaterial->addAction(p->addMatteMaterialAct);
  addMaterial->addAction(p->addPhongMaterialAct);
  addMaterial->addAction(p->addTransparentMaterialAct);
  addMaterial->addAction(p->addReflectiveMaterialAct);
  
  auto addTexture = p->editMenu->addMenu(tr("Add Texture"));
  addTexture->addAction(p->addConstantColorTextureAct);
  addTexture->addAction(p->addCheckerBoardTextureAct);

  auto addLight = p->editMenu->addMenu(tr("Add Light"));
  addLight->addAction(p->addDirectionalLightAct);
  addLight->addAction(p->addPointLightAct);

  auto addCamera = p->editMenu->addMenu(tr("Add Camera"));
  addCamera->addAction(p->addPinholeCameraAct);
  addCamera->addAction(p->addFishEyeCameraAct);
  addCamera->addAction(p->addOrthographicCameraAct);
  addCamera->addAction(p->addSphericalCameraAct);
  
  p->editMenu->addSeparator();
  p->editMenu->addAction(p->deleteElementAct);
  
  p->editMenu->addSeparator();
  auto move = p->editMenu->addMenu(tr("Move"));
  move->addAction(p->moveForwardsAlongXAct);
  move->addAction(p->moveBackwardsAlongXAct);
  move->addAction(p->moveForwardsAlongYAct);
  move->addAction(p->moveBackwardsAlongYAct);
  move->addAction(p->moveForwardsAlongZAct);
  move->addAction(p->moveBackwardsAlongZAct);

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

void MainWindow::addUnion() {
  add<Union>();
}

void MainWindow::addDifference() {
  add<Difference>();
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

void MainWindow::addDirectionalLight() {
  add<DirectionalLight>();
}

void MainWindow::addPointLight() {
  add<PointLight>();
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

void MainWindow::moveTransformable(const Vector3d& vec) {
  if (auto t = dynamic_cast<Transformable*>(p->currentElement)) {
    setFocus();
    t->moveBy(vec * 0.1, QGuiApplication::keyboardModifiers() & Qt::ShiftModifier);
    elementChanged(t);
  }
}

void MainWindow::moveForwardsAlongX() {
  moveTransformable(Vector3d::right());
}

void MainWindow::moveBackwardsAlongX() {
  moveTransformable(-Vector3d::right());
}

void MainWindow::moveForwardsAlongY() {
  moveTransformable(Vector3d::up());
}

void MainWindow::moveBackwardsAlongY() {
  moveTransformable(-Vector3d::up());
}

void MainWindow::moveForwardsAlongZ() {
  moveTransformable(Vector3d::forward());
}

void MainWindow::moveBackwardsAlongZ() {
  moveTransformable(-Vector3d::forward());
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
  p->propertyEditorWidget->update();
  updateWindowModified();
  redraw();
  emit currentElementChanged();
}

void MainWindow::elementSelected(const QModelIndex& current, const QModelIndex&) {
  auto element = static_cast<Element*>(current.internalPointer());
  p->currentElement = element;
  p->currentIndex = current;
  p->deleteElementAct->setEnabled(element != nullptr && dynamic_cast<Scene*>(element) == nullptr);
  
  auto transformable = dynamic_cast<Transformable*>(element);
  p->moveForwardsAlongXAct->setEnabled(transformable != nullptr);
  p->moveBackwardsAlongXAct->setEnabled(transformable != nullptr);
  p->moveForwardsAlongYAct->setEnabled(transformable != nullptr);
  p->moveBackwardsAlongYAct->setEnabled(transformable != nullptr);
  p->moveForwardsAlongZAct->setEnabled(transformable != nullptr);
  p->moveBackwardsAlongZAct->setEnabled(transformable != nullptr);
  
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

  setFocus();
  p->propertyEditorWidget->update();

  updateWindowModified();
}

void MainWindow::redraw() {
  p->display->setScene(p->scene);
}

#include "MainWindow.moc"
