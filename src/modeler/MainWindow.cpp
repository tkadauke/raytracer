#include <QGuiApplication>
#include <QComboBox>
#include <QInputDialog>

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QDockWidget>
#include <QTreeView>
#include <QItemSelectionModel>

#include <QMouseEvent>

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>

#include <algorithm>
#include <exception>
#include <utility>

#include "MainWindow.h"
#include "Display.h"
#include "engine/graph/RenderGraphCompiler.h"
#include "engine/raytracer/Raytracer.h"
#include "render/tonemap/TonemapFactory.h"
#include "render/viewplanes/ViewPlane.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"
#include "render/lights/PointLight.h"
#include "render/cameras/PinholeCamera.h"
#include "core/math/HitPointInterval.h"

#include "widgets/world/PropertyEditorWidget.h"
#include "widgets/world/PreviewDisplayWidget.h"
#include "widgets/world/RenderGraphInspectorWidget.h"
#include "widgets/world/SceneModel.h"
#include "widgets/world/RenderWindow.h"

#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"
#include "world/objects/Box.h"
#include "world/objects/Cylinder.h"
#include "world/objects/Ring.h"
#include "world/objects/Torus.h"
#include "world/objects/ScriptedSurface.h"

#include "world/objects/Intersection.h"
#include "world/objects/Union.h"
#include "world/objects/Difference.h"
#include "world/objects/MinkowskiSum.h"
#include "world/objects/ConvexHull.h"

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
#include "world/objects/ThinLensCamera.h"
#include "world/objects/TiltShiftCamera.h"
#include "world/objects/EquirectangularCamera.h"

struct MainWindow::Private {
  inline Private()
      : timelineDockWidget(nullptr),
        timelineFrameSlider(nullptr),
        timelineFrameSpinBox(nullptr),
        timelineSummaryLabel(nullptr),
        renderGraphDockWidget(nullptr),
        renderGraphInspectorWidget(nullptr),
        currentFrame(0),
        currentElement(nullptr) {
  }

  QString fileName;

  RenderDisplay* display;
  PreviewDisplayWidget* materialDisplay;
  PropertyEditorWidget* propertyEditorWidget;
  SceneModel* elementModel;
  QDockWidget* timelineDockWidget;
  QSlider* timelineFrameSlider;
  QSpinBox* timelineFrameSpinBox;
  QLabel* timelineSummaryLabel;
  QDockWidget* renderGraphDockWidget;
  RenderGraphInspectorWidget* renderGraphInspectorWidget;

  RenderWindow* renderWindow;

  Scene* scene;
  int currentFrame;

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
  QAction* addCylinderAct;
  QAction* addRingAct;
  QAction* addTorusAct;
  QAction* addScriptAct;

  QAction* addIntersectionAct;
  QAction* addUnionAct;
  QAction* addDifferenceAct;
  QAction* addMinkowskiSumAct;
  QAction* addConvexHullAct;

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
  QAction* addThinLensCameraAct;
  QAction* addTiltShiftCameraAct;
  QAction* addEquirectangularCameraAct;

  QAction* deleteElementAct;

  QAction* moveForwardsAlongXAct;
  QAction* moveBackwardsAlongXAct;
  QAction* moveForwardsAlongYAct;
  QAction* moveBackwardsAlongYAct;
  QAction* moveForwardsAlongZAct;
  QAction* moveBackwardsAlongZAct;

  QAction* renderAct;
  QAction* previewRaytracerAct;
  QAction* previewWireframeAct;
  QAction* previewRasterizerAct;
  QAction* previewRasterizerShadowsAct;
  QAction* previewPostAANoneAct;
  QAction* previewPostAAFxaaAct;
  QAction* previewPostAASmaaAct;
  QAction* previewWireframeOverlayAct;
  QAction* previewTonemapLinearAct;
  QAction* previewTonemapReinhardAct;
  QAction* previewTonemapAcesAct;

  QAction* aspectStretchAct;
  QAction* aspectFitWidthAct;
  QAction* aspectFitHeightAct;
  QAction* aspectFitExactAct;
  QMenu* aspectRatioMenu;
  QAction* aspect16x9Act;
  QAction* aspect4x3Act;
  QAction* aspect1x1Act;
  QAction* aspect239x1Act;
  QAction* aspect21x9Act;

  QAction* aboutAct;
  QAction* helpAct;
};

MainWindow::~MainWindow() = default;

MainWindow::MainWindow()
    : QMainWindow(),
      p(std::make_unique<Private>()) {
  p->scene = new ::Scene(nullptr);

  p->display = new RenderDisplay(this);
  setCentralWidget(p->display);

  addDockWidget(Qt::LeftDockWidgetArea, createElementSelector());
  addDockWidget(Qt::RightDockWidgetArea, createPropertyEditor());
  addDockWidget(Qt::RightDockWidgetArea, createPreviewDisplay());
  addDockWidget(Qt::BottomDockWidgetArea, createTimelineControls());
  addDockWidget(Qt::BottomDockWidgetArea, createRenderGraphInspector());

  connect(this, SIGNAL(selectionChanged(Element*)), this, SLOT(updatePreviewWidget()));
  connect(this, SIGNAL(currentElementChanged()), this, SLOT(updatePreviewWidget()));
  connect(p->display, SIGNAL(renderGraphInputsChanged()), this, SLOT(updateRenderGraphInspector()));
  connect(p->display, &RenderDisplay::renderGraphExecutionStarted, p->renderGraphInspectorWidget,
          &RenderGraphInspectorWidget::clearExecutionState);
  connect(p->display, &RenderDisplay::renderGraphPassStarted, p->renderGraphInspectorWidget,
          &RenderGraphInspectorWidget::passExecutionStarted);
  connect(p->display, &RenderDisplay::renderGraphPassFinished, p->renderGraphInspectorWidget,
          &RenderGraphInspectorWidget::passExecutionFinished);
  connect(p->display, &RenderDisplay::renderGraphPassFailed, p->renderGraphInspectorWidget,
          &RenderGraphInspectorWidget::passExecutionFailed);
  connect(p->display, &RenderWidget::renderFailed, this, [this](const QString& message) {
    statusBar()->showMessage(tr("Preview render failed: %1").arg(message));
  });
  connect(p->renderGraphInspectorWidget, SIGNAL(overridesChanged()), this,
          SLOT(renderGraphOverridesChanged()));

  createActions();
  createMenus();

  p->renderWindow = new RenderWindow(nullptr);
  resetTimelineFrame();
  updateRenderGraphInspector();
  p->display->setScene(p->scene);
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

  p->addCylinderAct = new QAction(tr("Cylinder"), this);
  p->addCylinderAct->setStatusTip(tr("Add a Cylinder to the scene"));
  connect(p->addCylinderAct, SIGNAL(triggered()), this, SLOT(addCylinder()));

  p->addRingAct = new QAction(tr("Ring"), this);
  p->addRingAct->setStatusTip(tr("Add a Ring to the scene"));
  connect(p->addRingAct, SIGNAL(triggered()), this, SLOT(addRing()));

  p->addTorusAct = new QAction(tr("Torus"), this);
  p->addTorusAct->setStatusTip(tr("Add a Torus to the scene"));
  connect(p->addTorusAct, SIGNAL(triggered()), this, SLOT(addTorus()));

  p->addScriptAct = new QAction(tr("Script"), this);
  p->addScriptAct->setStatusTip(tr("Add a Script to the scene"));
  connect(p->addScriptAct, SIGNAL(triggered()), this, SLOT(addScript()));

  p->addIntersectionAct = new QAction(tr("Intersection"), this);
  p->addIntersectionAct->setStatusTip(tr("Add an intersection to the scene"));
  connect(p->addIntersectionAct, SIGNAL(triggered()), this, SLOT(addIntersection()));

  p->addUnionAct = new QAction(tr("Union"), this);
  p->addUnionAct->setStatusTip(tr("Add a union to the scene"));
  connect(p->addUnionAct, SIGNAL(triggered()), this, SLOT(addUnion()));

  p->addDifferenceAct = new QAction(tr("Difference"), this);
  p->addDifferenceAct->setStatusTip(tr("Add a difference to the scene"));
  connect(p->addDifferenceAct, SIGNAL(triggered()), this, SLOT(addDifference()));

  p->addMinkowskiSumAct = new QAction(tr("Minkowski Sum"), this);
  p->addMinkowskiSumAct->setStatusTip(tr("Add a Minkowski sum to the scene"));
  connect(p->addMinkowskiSumAct, SIGNAL(triggered()), this, SLOT(addMinkowskiSum()));

  p->addConvexHullAct = new QAction(tr("Convex Hull"), this);
  p->addConvexHullAct->setStatusTip(tr("Add a Convex Hull to the scene"));
  connect(p->addConvexHullAct, SIGNAL(triggered()), this, SLOT(addConvexHull()));

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
  connect(p->addConstantColorTextureAct, SIGNAL(triggered()), this,
          SLOT(addConstantColorTexture()));

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

  p->addThinLensCameraAct = new QAction(tr("Thin Lens Camera (DOF)"), this);
  p->addThinLensCameraAct->setStatusTip(
    tr("Add a thin-lens camera with depth-of-field to the scene"));
  connect(p->addThinLensCameraAct, SIGNAL(triggered()), this, SLOT(addThinLensCamera()));

  p->addTiltShiftCameraAct = new QAction(tr("Tilt-Shift Camera"), this);
  p->addTiltShiftCameraAct->setStatusTip(
    tr("Add a tilt-shift / Scheimpflug camera (DOF + tilted focal plane) to the scene"));
  connect(p->addTiltShiftCameraAct, SIGNAL(triggered()), this, SLOT(addTiltShiftCamera()));

  p->addEquirectangularCameraAct = new QAction(tr("Equirectangular Camera (360°)"), this);
  p->addEquirectangularCameraAct->setStatusTip(
    tr("Add a full-sphere panorama camera to the scene (render at 2:1 aspect)"));
  connect(p->addEquirectangularCameraAct, SIGNAL(triggered()), this,
          SLOT(addEquirectangularCamera()));

  p->moveForwardsAlongXAct = new QAction(tr("Move forwards along X axis"), this);
  p->moveForwardsAlongXAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::META | Qt::Key_Up), QKeySequence(Qt::SHIFT | Qt::META | Qt::Key_Up)});
  p->moveForwardsAlongXAct->setStatusTip(tr("Moves the current element forwards along the X axis"));
  connect(p->moveForwardsAlongXAct, SIGNAL(triggered()), this, SLOT(moveForwardsAlongX()));

  p->moveBackwardsAlongXAct = new QAction(tr("Move backwards along X axis"), this);
  p->moveBackwardsAlongXAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::META | Qt::Key_Down), QKeySequence(Qt::SHIFT | Qt::META | Qt::Key_Down)});
  p->moveBackwardsAlongXAct->setStatusTip(
    tr("Moves the current element backwards along the X axis"));
  connect(p->moveBackwardsAlongXAct, SIGNAL(triggered()), this, SLOT(moveBackwardsAlongX()));

  p->moveForwardsAlongYAct = new QAction(tr("Move forwards along Y axis"), this);
  p->moveForwardsAlongYAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::ALT | Qt::Key_Up), QKeySequence(Qt::SHIFT | Qt::ALT | Qt::Key_Up)});
  p->moveForwardsAlongYAct->setStatusTip(tr("Moves the current element forwards along the Y axis"));
  connect(p->moveForwardsAlongYAct, SIGNAL(triggered()), this, SLOT(moveForwardsAlongY()));

  p->moveBackwardsAlongYAct = new QAction(tr("Move backwards along Y axis"), this);
  p->moveBackwardsAlongYAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::ALT | Qt::Key_Down), QKeySequence(Qt::SHIFT | Qt::ALT | Qt::Key_Down)});
  p->moveBackwardsAlongYAct->setStatusTip(
    tr("Moves the current element backwards along the Y axis"));
  connect(p->moveBackwardsAlongYAct, SIGNAL(triggered()), this, SLOT(moveBackwardsAlongY()));

  p->moveForwardsAlongZAct = new QAction(tr("Move forwards along Z axis"), this);
  p->moveForwardsAlongZAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::CTRL | Qt::Key_Up), QKeySequence(Qt::SHIFT | Qt::CTRL | Qt::Key_Up)});
  p->moveForwardsAlongZAct->setStatusTip(tr("Moves the current element forwards along the Z axis"));
  connect(p->moveForwardsAlongZAct, SIGNAL(triggered()), this, SLOT(moveForwardsAlongZ()));

  p->moveBackwardsAlongZAct = new QAction(tr("Move backwards along Z axis"), this);
  p->moveBackwardsAlongZAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::CTRL | Qt::Key_Down), QKeySequence(Qt::SHIFT | Qt::CTRL | Qt::Key_Down)});
  p->moveBackwardsAlongZAct->setStatusTip(
    tr("Moves the current element backwards along the Z axis"));
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
  p->renderAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
  p->renderAct->setStatusTip(tr("Render current scene"));
  connect(p->renderAct, SIGNAL(triggered()), this, SLOT(render()));

  // Preview-engine selection — radio-style via a QActionGroup so
  // exactly one is active at a time. Defaults to Raytracer to match
  // the historical behaviour.
  p->previewRaytracerAct = new QAction(tr("&Raytracer"), this);
  p->previewRaytracerAct->setStatusTip(tr("Show the modeling preview as a ray-traced render"));
  p->previewRaytracerAct->setCheckable(true);
  p->previewRaytracerAct->setChecked(true);
  connect(p->previewRaytracerAct, SIGNAL(triggered()), this, SLOT(usePreviewRaytracer()));

  p->previewWireframeAct = new QAction(tr("&Wireframe"), this);
  p->previewWireframeAct->setStatusTip(
    tr("Show the modeling preview as a wireframe (faster, geometry-only)"));
  p->previewWireframeAct->setCheckable(true);
  connect(p->previewWireframeAct, SIGNAL(triggered()), this, SLOT(usePreviewWireframe()));

  p->previewRasterizerAct = new QAction(tr("Ras&terizer"), this);
  p->previewRasterizerAct->setStatusTip(
    tr("Show the modeling preview as a software-rasterized render (filled triangles, Lambertian "
       "shading, no recursion)"));
  p->previewRasterizerAct->setCheckable(true);
  connect(p->previewRasterizerAct, SIGNAL(triggered()), this, SLOT(usePreviewRasterizer()));

  p->previewRasterizerShadowsAct = new QAction(tr("Rasterizer Preview &Shadows"), this);
  p->previewRasterizerShadowsAct->setStatusTip(
    tr("Enable directional shadow maps in the live rasterizer preview"));
  p->previewRasterizerShadowsAct->setCheckable(true);
  connect(p->previewRasterizerShadowsAct, SIGNAL(triggered(bool)), this,
          SLOT(setPreviewRasterizerShadows(bool)));

  p->previewPostAANoneAct = new QAction(tr("&None"), this);
  p->previewPostAANoneAct->setStatusTip(
    tr("Disable image-space anti-aliasing in the live preview"));
  p->previewPostAANoneAct->setCheckable(true);
  p->previewPostAANoneAct->setChecked(true);
  connect(p->previewPostAANoneAct, SIGNAL(triggered()), this, SLOT(setPreviewPostAANone()));

  p->previewPostAAFxaaAct = new QAction(tr("&FXAA"), this);
  p->previewPostAAFxaaAct->setStatusTip(tr("Add a graph-visible FXAA pass to the live preview"));
  p->previewPostAAFxaaAct->setCheckable(true);
  connect(p->previewPostAAFxaaAct, SIGNAL(triggered()), this, SLOT(setPreviewPostAAFxaa()));

  p->previewPostAASmaaAct = new QAction(tr("&SMAA"), this);
  p->previewPostAASmaaAct->setStatusTip(tr("Add a graph-visible SMAA pass to the live preview"));
  p->previewPostAASmaaAct->setCheckable(true);
  connect(p->previewPostAASmaaAct, SIGNAL(triggered()), this, SLOT(setPreviewPostAASmaa()));

  auto previewPostAAGroup = new QActionGroup(this);
  previewPostAAGroup->addAction(p->previewPostAANoneAct);
  previewPostAAGroup->addAction(p->previewPostAAFxaaAct);
  previewPostAAGroup->addAction(p->previewPostAASmaaAct);

  p->previewWireframeOverlayAct = new QAction(tr("Wireframe &Overlay"), this);
  p->previewWireframeOverlayAct->setStatusTip(
    tr("Draw graph-generated wireframe edges over the live shaded preview"));
  p->previewWireframeOverlayAct->setCheckable(true);
  connect(p->previewWireframeOverlayAct, SIGNAL(triggered(bool)), this,
          SLOT(setPreviewWireframeOverlay(bool)));

  auto previewGroup = new QActionGroup(this);
  previewGroup->addAction(p->previewRaytracerAct);
  previewGroup->addAction(p->previewWireframeAct);
  previewGroup->addAction(p->previewRasterizerAct);

  p->previewTonemapLinearAct = new QAction(tr("&Linear"), this);
  p->previewTonemapLinearAct->setStatusTip(tr("Use the linear preview tonemap"));
  p->previewTonemapLinearAct->setCheckable(true);
  p->previewTonemapLinearAct->setChecked(true);
  connect(p->previewTonemapLinearAct, SIGNAL(triggered()), this, SLOT(setPreviewTonemapLinear()));

  p->previewTonemapReinhardAct = new QAction(tr("&Reinhard"), this);
  p->previewTonemapReinhardAct->setStatusTip(tr("Use the Reinhard preview tonemap"));
  p->previewTonemapReinhardAct->setCheckable(true);
  connect(p->previewTonemapReinhardAct, SIGNAL(triggered()), this,
          SLOT(setPreviewTonemapReinhard()));

  p->previewTonemapAcesAct = new QAction(tr("&ACES"), this);
  p->previewTonemapAcesAct->setStatusTip(tr("Use the ACES preview tonemap"));
  p->previewTonemapAcesAct->setCheckable(true);
  connect(p->previewTonemapAcesAct, SIGNAL(triggered()), this, SLOT(setPreviewTonemapAces()));

  auto previewTonemapGroup = new QActionGroup(this);
  previewTonemapGroup->addAction(p->previewTonemapLinearAct);
  previewTonemapGroup->addAction(p->previewTonemapReinhardAct);
  previewTonemapGroup->addAction(p->previewTonemapAcesAct);

  p->aspectStretchAct = new QAction(tr("&Stretch"), this);
  p->aspectStretchAct->setStatusTip(tr("Fill both axes independently (may distort geometry)"));
  p->aspectStretchAct->setCheckable(true);
  connect(p->aspectStretchAct, SIGNAL(triggered()), this, SLOT(setAspectStretch()));

  p->aspectFitWidthAct = new QAction(tr("Fit &Width"), this);
  p->aspectFitWidthAct->setStatusTip(
    tr("Horizontal FOV constant, vertical derived from window (square pixels, no distortion)"));
  p->aspectFitWidthAct->setCheckable(true);
  p->aspectFitWidthAct->setChecked(true);
  connect(p->aspectFitWidthAct, SIGNAL(triggered()), this, SLOT(setAspectFitWidth()));

  p->aspectFitHeightAct = new QAction(tr("Fit &Height"), this);
  p->aspectFitHeightAct->setStatusTip(
    tr("Vertical FOV constant, horizontal derived from window (square pixels, no distortion)"));
  p->aspectFitHeightAct->setCheckable(true);
  connect(p->aspectFitHeightAct, SIGNAL(triggered()), this, SLOT(setAspectFitHeight()));

  p->aspectFitExactAct = new QAction(tr("Fit &Exact (letterbox)"), this);
  p->aspectFitExactAct->setStatusTip(
    tr("Fixed intrinsic aspect ratio with black bars for the remainder"));
  p->aspectFitExactAct->setCheckable(true);
  connect(p->aspectFitExactAct, SIGNAL(triggered()), this, SLOT(setAspectFitExact()));

  auto aspectGroup = new QActionGroup(this);
  aspectGroup->addAction(p->aspectStretchAct);
  aspectGroup->addAction(p->aspectFitWidthAct);
  aspectGroup->addAction(p->aspectFitHeightAct);
  aspectGroup->addAction(p->aspectFitExactAct);

  p->aspect16x9Act = new QAction(tr("16:9"), this);
  connect(p->aspect16x9Act, SIGNAL(triggered()), this, SLOT(setAspectRatio16x9()));

  p->aspect4x3Act = new QAction(tr("4:3"), this);
  connect(p->aspect4x3Act, SIGNAL(triggered()), this, SLOT(setAspectRatio4x3()));

  p->aspect1x1Act = new QAction(tr("1:1"), this);
  connect(p->aspect1x1Act, SIGNAL(triggered()), this, SLOT(setAspectRatio1x1()));

  p->aspect239x1Act = new QAction(tr("2.39:1 (CinemaScope)"), this);
  connect(p->aspect239x1Act, SIGNAL(triggered()), this, SLOT(setAspectRatio239x1()));

  p->aspect21x9Act = new QAction(tr("21:9 (Ultrawide)"), this);
  connect(p->aspect21x9Act, SIGNAL(triggered()), this, SLOT(setAspectRatio21x9()));

  p->helpAct = new QAction(tr("Raytracer &Help"), this);
  p->helpAct->setStatusTip(tr("Go to the Github page"));
  connect(p->helpAct, SIGNAL(triggered()), this, SLOT(help()));

  auto modifyingActions = {p->newAct,    p->openAct,   p->saveAct,
                           p->saveAsAct, p->addBoxAct, p->deleteElementAct};

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
  addPrimitive->addAction(p->addCylinderAct);
  addPrimitive->addAction(p->addRingAct);
  addPrimitive->addAction(p->addTorusAct);
  addPrimitive->addAction(p->addScriptAct);

  auto addComposite = p->editMenu->addMenu(tr("Add Composite"));
  addComposite->addAction(p->addIntersectionAct);
  addComposite->addAction(p->addUnionAct);
  addComposite->addAction(p->addDifferenceAct);
  addComposite->addAction(p->addMinkowskiSumAct);
  addComposite->addAction(p->addConvexHullAct);

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
  addCamera->addAction(p->addThinLensCameraAct);
  addCamera->addAction(p->addTiltShiftCameraAct);
  addCamera->addAction(p->addEquirectangularCameraAct);

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
  p->renderMenu->addSeparator();
  auto previewMenu = p->renderMenu->addMenu(tr("&Preview Engine"));
  previewMenu->addAction(p->previewRaytracerAct);
  previewMenu->addAction(p->previewWireframeAct);
  previewMenu->addAction(p->previewRasterizerAct);
  previewMenu->addSeparator();
  previewMenu->addAction(p->previewWireframeOverlayAct);
  previewMenu->addAction(p->previewRasterizerShadowsAct);
  auto previewPostAAMenu = previewMenu->addMenu(tr("Preview Post &AA"));
  previewPostAAMenu->addAction(p->previewPostAANoneAct);
  previewPostAAMenu->addAction(p->previewPostAAFxaaAct);
  previewPostAAMenu->addAction(p->previewPostAASmaaAct);

  auto previewTonemapMenu = p->renderMenu->addMenu(tr("Preview &Tonemap"));
  previewTonemapMenu->addAction(p->previewTonemapLinearAct);
  previewTonemapMenu->addAction(p->previewTonemapReinhardAct);
  previewTonemapMenu->addAction(p->previewTonemapAcesAct);

  p->renderMenu->addSeparator();
  auto aspectModeMenu = p->renderMenu->addMenu(tr("&Aspect Mode"));
  aspectModeMenu->addAction(p->aspectStretchAct);
  aspectModeMenu->addAction(p->aspectFitWidthAct);
  aspectModeMenu->addAction(p->aspectFitHeightAct);
  aspectModeMenu->addAction(p->aspectFitExactAct);

  p->aspectRatioMenu = p->renderMenu->addMenu(tr("Aspect &Ratio"));
  p->aspectRatioMenu->setEnabled(false);
  p->aspectRatioMenu->addAction(p->aspect16x9Act);
  p->aspectRatioMenu->addAction(p->aspect4x3Act);
  p->aspectRatioMenu->addAction(p->aspect1x1Act);
  p->aspectRatioMenu->addAction(p->aspect239x1Act);
  p->aspectRatioMenu->addAction(p->aspect21x9Act);

  p->helpMenu = menuBar()->addMenu(tr("&Help"));
  p->helpMenu->addAction(p->aboutAct);
  p->helpMenu->addAction(p->helpAct);
}

bool MainWindow::maybeSave() {
  if (p->scene->changed()) {
    auto response = QMessageBox::question(
      this, tr("Save changes?"),
      tr("There are unsaved changes to this document. Would you like to save them?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);

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

    p->elementModel->setElement(p->scene);
    resetTimelineFrame();
    redraw();
  }
}

void MainWindow::openFile() {
  QString fileName =
    QFileDialog::getOpenFileName(this, tr("Open File"), QString(), tr("Scenes (*.json)"));

  if (!fileName.isNull() && maybeSave()) {
    if (p->scene)
      delete p->scene;

    p->fileName = QString();
    p->currentElement = nullptr;
    emit selectionChanged(nullptr);

    p->scene = new ::Scene(nullptr);
    p->scene->load(fileName);
    p->fileName = fileName;
    p->propertyEditorWidget->setRoot(p->scene);
    p->elementModel->setElement(p->scene);

    resetTimelineFrame();
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
  QString fileName =
    QFileDialog::getSaveFileName(this, tr("Save File"), p->fileName, tr("Scenes (*.json)"));

  if (!fileName.isNull()) {
    p->fileName = fileName;
    p->scene->save(p->fileName);
  }
}

template<class T>
void MainWindow::add() {
  auto element = new T(nullptr);
  element->setName(
    QString("%1 %2").arg(element->metaObject()->className()).arg(p->scene->childElements().size()));

  p->elementModel->addElement(p->currentIndex, element);
  elementChanged(element);
}

void MainWindow::addBox() {
  add<Box>();
}

void MainWindow::addSphere() {
  add<Sphere>();
}

void MainWindow::addCylinder() {
  add<Cylinder>();
}

void MainWindow::addRing() {
  add<Ring>();
}

void MainWindow::addTorus() {
  add<Torus>();
}

void MainWindow::addScript() {
  add<ScriptedSurface>();
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

void MainWindow::addMinkowskiSum() {
  add<MinkowskiSum>();
}

void MainWindow::addConvexHull() {
  add<ConvexHull>();
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

void MainWindow::addThinLensCamera() {
  add<ThinLensCamera>();
}

void MainWindow::addTiltShiftCamera() {
  add<TiltShiftCamera>();
}

void MainWindow::addEquirectangularCamera() {
  add<EquirectangularCamera>();
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
    try {
      auto evaluatedScene = evaluatedSceneForCurrentFrame();
      p->renderWindow->setScene(evaluatedScene ? evaluatedScene.get() : p->scene);
      statusBar()->clearMessage();
    } catch (const std::exception& error) {
      statusBar()->showMessage(tr("Animation render failed: %1").arg(error.what()));
      return;
    }
  }

  p->renderWindow->show();
}

void MainWindow::usePreviewRaytracer() {
  p->display->setEngineKind(RenderDisplay::EngineKind::Raytracer);
}

void MainWindow::usePreviewRasterizer() {
  p->display->setEngineKind(RenderDisplay::EngineKind::Rasterizer);
}

void MainWindow::usePreviewWireframe() {
  p->display->setEngineKind(RenderDisplay::EngineKind::Wireframe);
}

void MainWindow::setPreviewRasterizerShadows(bool enabled) {
  if (enabled) {
    p->previewRasterizerAct->setChecked(true);
    p->display->setEngineKind(RenderDisplay::EngineKind::Rasterizer);
  }
  p->display->setRasterizerPreviewShadowsEnabled(enabled);
}

void MainWindow::setPreviewPostAANone() {
  p->display->setPreviewPostProcessAA(engine::graph::RenderPostProcessAA::None);
}

void MainWindow::setPreviewPostAAFxaa() {
  p->display->setPreviewPostProcessAA(engine::graph::RenderPostProcessAA::FXAA);
}

void MainWindow::setPreviewPostAASmaa() {
  p->display->setPreviewPostProcessAA(engine::graph::RenderPostProcessAA::SMAA);
}

void MainWindow::setPreviewWireframeOverlay(bool enabled) {
  p->display->setWireframeOverlayEnabled(enabled);
}

void MainWindow::setPreviewTonemapLinear() {
  setPreviewTonemap("Linear");
}

void MainWindow::setPreviewTonemapReinhard() {
  setPreviewTonemap("Reinhard");
}

void MainWindow::setPreviewTonemapAces() {
  setPreviewTonemap("ACES");
}

void MainWindow::setAspectStretch() {
  p->display->setAspectMode(render::AspectMode::Stretch);
  p->aspectRatioMenu->setEnabled(false);
  p->display->render();
}

void MainWindow::setAspectFitWidth() {
  p->display->setAspectMode(render::AspectMode::FitWidth);
  p->aspectRatioMenu->setEnabled(false);
  p->display->render();
}

void MainWindow::setAspectFitHeight() {
  p->display->setAspectMode(render::AspectMode::FitHeight);
  p->aspectRatioMenu->setEnabled(false);
  p->display->render();
}

void MainWindow::setAspectFitExact() {
  p->display->setAspectMode(render::AspectMode::FitExact);
  p->aspectRatioMenu->setEnabled(true);
  p->display->render();
}

void MainWindow::setAspectRatio16x9() {
  p->display->setAspectRatio(16.0 / 9.0);
  p->aspectFitExactAct->setChecked(true);
  p->aspectRatioMenu->setEnabled(true);
  p->display->render();
}

void MainWindow::setAspectRatio4x3() {
  p->display->setAspectRatio(4.0 / 3.0);
  p->aspectFitExactAct->setChecked(true);
  p->aspectRatioMenu->setEnabled(true);
  p->display->render();
}

void MainWindow::setAspectRatio1x1() {
  p->display->setAspectRatio(1.0);
  p->aspectFitExactAct->setChecked(true);
  p->aspectRatioMenu->setEnabled(true);
  p->display->render();
}

void MainWindow::setAspectRatio239x1() {
  p->display->setAspectRatio(2.39);
  p->aspectFitExactAct->setChecked(true);
  p->aspectRatioMenu->setEnabled(true);
  p->display->render();
}

void MainWindow::setAspectRatio21x9() {
  p->display->setAspectRatio(21.0 / 9.0);
  p->aspectFitExactAct->setChecked(true);
  p->aspectRatioMenu->setEnabled(true);
  p->display->render();
}

void MainWindow::about() {
  QMessageBox::about(this, tr("About"), tr("This is the Modeler for the Raytracer library."));
}

void MainWindow::help() {
  QDesktopServices::openUrl(QUrl("https://github.com/tkadauke/raytracer"));
}

QDockWidget* MainWindow::createPropertyEditor() {
  p->propertyEditorWidget = new PropertyEditorWidget(p->scene, this);

  connect(p->propertyEditorWidget, SIGNAL(changed(Element*)), this, SLOT(elementChanged(Element*)));

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

  connect(itemSelectionModel, SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)), this,
          SLOT(elementSelected(const QModelIndex&, const QModelIndex&)));

  connect(p->elementModel, SIGNAL(rowsMoved(const QModelIndex&, int, int, const QModelIndex&, int)),
          this, SLOT(reorder()));

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

QDockWidget* MainWindow::createTimelineControls() {
  auto widget = new QWidget(this);
  auto layout = new QHBoxLayout(widget);

  auto frameLabel = new QLabel(tr("Frame"), widget);
  p->timelineFrameSlider = new QSlider(Qt::Horizontal, widget);
  p->timelineFrameSpinBox = new QSpinBox(widget);
  p->timelineSummaryLabel = new QLabel(widget);

  p->timelineFrameSpinBox->setKeyboardTracking(false);
  p->timelineFrameSpinBox->setFixedWidth(90);

  layout->addWidget(frameLabel);
  layout->addWidget(p->timelineFrameSlider, 1);
  layout->addWidget(p->timelineFrameSpinBox);
  layout->addWidget(p->timelineSummaryLabel);
  widget->setLayout(layout);

  connect(p->timelineFrameSlider, SIGNAL(valueChanged(int)), this, SLOT(setCurrentFrame(int)));
  connect(p->timelineFrameSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setCurrentFrame(int)));

  auto dockWidget = new QDockWidget("Timeline", this);
  dockWidget->setWidget(widget);
  p->timelineDockWidget = dockWidget;

  return dockWidget;
}

QDockWidget* MainWindow::createRenderGraphInspector() {
  p->renderGraphInspectorWidget = new RenderGraphInspectorWidget(this);

  auto dockWidget = new QDockWidget("Render Graph", this);
  dockWidget->setWidget(p->renderGraphInspectorWidget);
  p->renderGraphDockWidget = dockWidget;

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

void MainWindow::updateRenderGraphInspector() {
  if (!p->renderGraphInspectorWidget || !p->display)
    return;

  const QSize target = p->display->bufferSize();
  const auto intent = previewRenderIntent();
  engine::graph::RenderGraphCompiler compiler;
  const engine::graph::RenderTargetSpec targetSpec{std::max(1, target.width()),
                                                   std::max(1, target.height()), 1};
  p->display->setRenderGraphIntent(intent);
  p->renderGraphInspectorWidget->setPlan(compiler.compile(targetSpec, intent));
  applyRenderGraphPreviewPlan();
}

void MainWindow::renderGraphOverridesChanged() {
  if (applyRenderGraphPreviewPlan())
    p->display->render();
}

void MainWindow::setCurrentFrame(int frame) {
  if (!p->scene->hasAnimation())
    return;

  const auto* timeline = p->scene->animation();
  p->currentFrame = std::clamp(frame, timeline->startFrame(), timeline->endFrame());
  syncTimelineControls();
  redraw();
}

void MainWindow::reorder() {
  redraw();
  p->scene->setChanged(true);

  setFocus();
  p->propertyEditorWidget->update();

  updateWindowModified();
}

void MainWindow::redraw() {
  try {
    auto evaluatedScene = evaluatedSceneForCurrentFrame();
    updateRenderGraphInspector();
    p->display->setScene(evaluatedScene ? evaluatedScene.get() : p->scene);
    statusBar()->clearMessage();
  } catch (const std::exception& error) {
    statusBar()->showMessage(tr("Animation preview failed: %1").arg(error.what()));
    p->display->setScene(p->scene);
  }
}

bool MainWindow::applyRenderGraphPreviewPlan() {
  if (!p->renderGraphInspectorWidget || !p->display)
    return false;

  const auto plan = p->renderGraphInspectorWidget->effectivePlan();
  const auto validation = plan.validate();
  if (!validation.valid()) {
    const auto& first = validation.errors().front();
    p->display->setRenderGraphPreviewEnabled(false);
    statusBar()->showMessage(
      tr("Render graph preview paused: %1").arg(QString::fromStdString(first.message)));
    return false;
  }

  p->display->setRenderGraphPlan(plan);
  statusBar()->clearMessage();
  return true;
}

void MainWindow::setPreviewTonemap(const std::string& name) {
  auto tonemap = render::TonemapFactory::self().createShared(name);
  if (!tonemap) {
    statusBar()->showMessage(
      tr("Preview tonemap is not registered: %1").arg(QString::fromStdString(name)));
    return;
  }

  p->display->setPreviewTonemap(std::move(tonemap));
}

void MainWindow::resetTimelineFrame() {
  if (const auto* timeline = p->scene->animation()) {
    p->currentFrame = timeline->startFrame();
  } else {
    p->currentFrame = 0;
  }
  syncTimelineControls();
}

void MainWindow::syncTimelineControls() {
  const auto* timeline = p->scene->animation();
  const bool hasAnimation = timeline != nullptr;

  p->timelineDockWidget->setEnabled(hasAnimation);
  {
    const QSignalBlocker sliderBlocker(p->timelineFrameSlider);
    const QSignalBlocker spinBoxBlocker(p->timelineFrameSpinBox);

    if (hasAnimation) {
      p->currentFrame = std::clamp(p->currentFrame, timeline->startFrame(), timeline->endFrame());
      p->timelineFrameSlider->setRange(timeline->startFrame(), timeline->endFrame());
      p->timelineFrameSpinBox->setRange(timeline->startFrame(), timeline->endFrame());
      p->timelineFrameSlider->setValue(p->currentFrame);
      p->timelineFrameSpinBox->setValue(p->currentFrame);
    } else {
      p->timelineFrameSlider->setRange(0, 0);
      p->timelineFrameSpinBox->setRange(0, 0);
      p->timelineFrameSlider->setValue(0);
      p->timelineFrameSpinBox->setValue(0);
    }
  }

  if (hasAnimation) {
    p->timelineSummaryLabel->setText(tr("%1-%2, %3 fps")
                                       .arg(timeline->startFrame())
                                       .arg(timeline->endFrame())
                                       .arg(timeline->fps()));
  } else {
    p->timelineSummaryLabel->setText(tr("No animation"));
  }
}

engine::graph::RenderIntent MainWindow::previewRenderIntent() const {
  engine::graph::RenderIntent intent = p->scene && p->scene->hasRenderIntent()
                                         ? p->scene->renderIntent()
                                         : engine::graph::RenderIntent();
  intent.enablePreviewShadows =
    intent.enablePreviewShadows || (p->display && p->display->rasterizerPreviewShadowsEnabled());
  if (p->display &&
      p->display->previewPostProcessAA() != engine::graph::RenderPostProcessAA::None) {
    intent.postProcessAA = p->display->previewPostProcessAA();
  }
  intent.enableWireframeOverlay =
    intent.enableWireframeOverlay || (p->display && p->display->wireframeOverlayEnabled());

  if (!p->display)
    return intent;

  switch (p->display->engineKind()) {
  case RenderDisplay::EngineKind::Raytracer:
    intent.defaultExecutor = engine::graph::RenderExecutorPreference::Raytracer;
    intent.defaultViewMode = engine::graph::RenderViewMode::Beauty;
    break;
  case RenderDisplay::EngineKind::Rasterizer:
    intent.defaultExecutor = engine::graph::RenderExecutorPreference::Rasterizer;
    intent.defaultViewMode = engine::graph::RenderViewMode::Beauty;
    break;
  case RenderDisplay::EngineKind::Wireframe:
    intent.defaultExecutor = engine::graph::RenderExecutorPreference::Wireframe;
    intent.defaultViewMode = engine::graph::RenderViewMode::Wireframe;
    break;
  }

  return intent;
}

std::unique_ptr<Scene> MainWindow::evaluatedSceneForCurrentFrame() const {
  if (!p->scene->hasAnimation())
    return nullptr;
  return p->scene->evaluatedAtFrame(p->currentFrame);
}
