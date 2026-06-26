#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "widgets/world/RenderGraphInspectorWidget.h"

#include "core/Buffer.h"
#include "core/math/Matrix.h"
#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RenderGraphCompiler.h"
#include "engine/graph/RenderGraphExecutionTrace.h"
#include "render/cameras/PinholeCamera.h"
#include "render/lights/PointLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Curve.h"
#include "render/primitives/Rectangle.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"
#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/Slot.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QJsonArray>
#include <QLabel>
#include <QToolButton>
#include <QTreeWidget>
#include <QWidget>

#include <chrono>
#include <thread>

namespace RenderGraphInspectorWidgetTest {
  using namespace engine::graph;

  class RenderGraphInspectorWidgetTest : public ::testing::GuiTest {};

  QGraphicsItem* graphItem(QGraphicsScene* scene, const QString& kind, const QString& id) {
    for (QGraphicsItem* item : scene->items()) {
      if (item->data(0).toString() == kind && item->data(1).toString() == id) {
        return item;
      }
    }
    return nullptr;
  }

  QGraphicsItem* graphNodeItem(QGraphicsScene* scene, const QString& kind, const QString& id) {
    for (QGraphicsItem* item : scene->items()) {
      if (!item->parentItem() && item->data(0).toString() == kind &&
          item->data(1).toString() == id) {
        return item;
      }
    }
    return nullptr;
  }

  bool nodeTextContains(QGraphicsItem* node, const QString& text) {
    if (!node)
      return false;

    for (QGraphicsItem* child : node->childItems()) {
      auto* label = dynamic_cast<QGraphicsSimpleTextItem*>(child);
      if (label && label->text().contains(text))
        return true;
    }
    return false;
  }

  bool nodeLineTooltipContains(QGraphicsItem* node, const QString& text) {
    if (!node)
      return false;

    for (QGraphicsItem* child : node->childItems()) {
      auto* label = dynamic_cast<QGraphicsSimpleTextItem*>(child);
      if (label && label->toolTip().contains(text))
        return true;
    }
    return false;
  }

  RenderPlan simplePlan() {
    RenderPlan plan;

    RenderResourceDescriptor resource;
    resource.id = "main_color";
    resource.name = "Main color";
    resource.type = RenderResourceType::Color;
    resource.format = RenderResourceFormat::RGBDouble;
    resource.width = 320;
    resource.height = 180;
    resource.sampleCount = 1;
    resource.lifetime = RenderResourceLifetime::Exported;
    plan.addResource(resource);

    RenderPassNode pass;
    pass.id = "raytrace_beauty";
    pass.name = "Raytraced beauty";
    pass.kind = RenderPassKind::Beauty;
    pass.executor = RenderExecutorKind::Raytracer;
    pass.writes.push_back({"main_color"});
    pass.sceneView.camera = RenderCameraRef{"preview-camera", std::nullopt};
    pass.sceneView.shadingProfile = ShadingProfileRef{"clay", {}};
    pass.disabledBehavior = DisabledBehavior::Error;
    plan.addPass(pass);

    return plan;
  }

  RenderPlan longLabelPlan() {
    RenderPlan plan = simplePlan();
    RenderPassNode pass = plan.passes().front();
    pass.sceneView.camera =
      RenderCameraRef{"preview-camera-with-a-very-long-readable-identifier", std::nullopt};
    pass.sceneView.shadingProfile =
      ShadingProfileRef{"technical-illustration-profile-with-long-name", {}};

    RenderPlan replacement;
    replacement.addResource(plan.resources().front());
    replacement.addPass(pass);
    return replacement;
  }

  RenderPlan twoPassPlan() {
    RenderPlan plan;

    RenderResourceDescriptor beauty;
    beauty.id = "beauty_color";
    beauty.name = "Beauty color";
    beauty.type = RenderResourceType::Color;
    beauty.format = RenderResourceFormat::RGBDouble;
    beauty.width = 320;
    beauty.height = 180;
    beauty.sampleCount = 1;
    beauty.lifetime = RenderResourceLifetime::Transient;
    plan.addResource(beauty);

    RenderResourceDescriptor display;
    display.id = "display_color";
    display.name = "Display color";
    display.type = RenderResourceType::Color;
    display.format = RenderResourceFormat::RGBDouble;
    display.width = 320;
    display.height = 180;
    display.sampleCount = 1;
    display.lifetime = RenderResourceLifetime::Exported;
    plan.addResource(display);

    RenderPassNode pass;
    pass.id = "raytrace_beauty";
    pass.name = "Raytraced beauty";
    pass.kind = RenderPassKind::Beauty;
    pass.executor = RenderExecutorKind::Raytracer;
    pass.writes.push_back({"beauty_color"});
    pass.disabledBehavior = DisabledBehavior::Error;
    plan.addPass(pass);

    RenderPassNode tonemap;
    tonemap.id = "tonemap";
    tonemap.name = "Tone map";
    tonemap.kind = RenderPassKind::Tonemap;
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.reads.push_back({"beauty_color"});
    tonemap.writes.push_back({"display_color"});
    tonemap.disabledBehavior = DisabledBehavior::Passthrough;
    plan.addPass(tonemap);

    return plan;
  }

  RenderPlan renderTextureScreenPlan() {
    RenderPlan plan;

    RenderResourceDescriptor subviewColor;
    subviewColor.id = "subview_monitor_feed_main_color";
    subviewColor.name = "Monitor feed main color";
    subviewColor.type = RenderResourceType::Color;
    subviewColor.format = RenderResourceFormat::RGBDouble;
    subviewColor.width = 64;
    subviewColor.height = 48;
    subviewColor.sampleCount = 1;
    subviewColor.lifetime = RenderResourceLifetime::Exported;
    subviewColor.features = {"subview",        "subview_monitor_feed", "render_to_texture",
                             "subview_output", "subview_color_output", "subview_name:monitor_feed"};
    plan.addResource(subviewColor);

    RenderResourceDescriptor subviewDepth;
    subviewDepth.id = "subview_monitor_feed_depth_aov";
    subviewDepth.name = "Monitor feed depth AOV";
    subviewDepth.type = RenderResourceType::Depth;
    subviewDepth.format = RenderResourceFormat::DepthDouble;
    subviewDepth.width = 64;
    subviewDepth.height = 48;
    subviewDepth.sampleCount = 1;
    subviewDepth.lifetime = RenderResourceLifetime::Exported;
    subviewDepth.features = {"subview",        "subview_monitor_feed", "render_to_texture",
                             "subview_output", "subview_depth_output", "subview_name:monitor_feed"};
    plan.addResource(subviewDepth);

    RenderResourceDescriptor beauty;
    beauty.id = "beauty_color";
    beauty.name = "Beauty color";
    beauty.type = RenderResourceType::Color;
    beauty.format = RenderResourceFormat::RGBDouble;
    beauty.width = 64;
    beauty.height = 48;
    beauty.sampleCount = 1;
    beauty.lifetime = RenderResourceLifetime::Transient;
    plan.addResource(beauty);

    RenderResourceDescriptor display;
    display.id = "main_color";
    display.name = "Main color";
    display.type = RenderResourceType::Color;
    display.format = RenderResourceFormat::RGBDouble;
    display.width = 64;
    display.height = 48;
    display.sampleCount = 1;
    display.lifetime = RenderResourceLifetime::Exported;
    plan.addResource(display);

    RenderPassNode subviewBeauty;
    subviewBeauty.id = "subview_monitor_feed_raster_beauty";
    subviewBeauty.name = "Monitor feed Raster beauty";
    subviewBeauty.kind = RenderPassKind::Beauty;
    subviewBeauty.executor = RenderExecutorKind::Rasterizer;
    subviewBeauty.features = {"subview", "subview_monitor_feed", "render_to_texture",
                              "subview_name:monitor_feed"};
    subviewBeauty.writes.push_back({"subview_monitor_feed_main_color"});
    subviewBeauty.disabledBehavior = DisabledBehavior::Error;
    plan.addPass(subviewBeauty);

    RenderPassNode subviewDepthPass;
    subviewDepthPass.id = "subview_monitor_feed_depth_aov";
    subviewDepthPass.name = "Monitor feed Depth AOV";
    subviewDepthPass.kind = RenderPassKind::AOV;
    subviewDepthPass.executor = RenderExecutorKind::Rasterizer;
    subviewDepthPass.features = {"subview", "subview_monitor_feed", "render_to_texture",
                                 "subview_name:monitor_feed"};
    subviewDepthPass.writes.push_back({"subview_monitor_feed_depth_aov"});
    subviewDepthPass.disabledBehavior = DisabledBehavior::SubstituteDefault;
    plan.addPass(subviewDepthPass);

    RenderPassNode finalBeauty;
    finalBeauty.id = "raytrace_beauty";
    finalBeauty.name = "Raytraced beauty";
    finalBeauty.kind = RenderPassKind::Beauty;
    finalBeauty.executor = RenderExecutorKind::Raytracer;
    finalBeauty.reads.push_back({"subview_monitor_feed_main_color"});
    finalBeauty.reads.push_back({"subview_monitor_feed_depth_aov"});
    finalBeauty.writes.push_back({"beauty_color"});
    finalBeauty.disabledBehavior = DisabledBehavior::Error;
    plan.addPass(finalBeauty);

    RenderPassNode tonemap;
    tonemap.id = "tonemap";
    tonemap.name = "Tone map";
    tonemap.kind = RenderPassKind::Tonemap;
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.reads.push_back({"beauty_color"});
    tonemap.writes.push_back({"main_color"});
    tonemap.disabledBehavior = DisabledBehavior::Passthrough;
    plan.addPass(tonemap);

    return plan;
  }

  RenderPlan featureGroupPlan() {
    RenderPlan plan = twoPassPlan();

    RenderPassNode postAA = plan.passes().back();
    postAA.id = "post_fxaa";
    postAA.name = "FXAA";
    postAA.kind = RenderPassKind::PostProcess;
    postAA.executor = RenderExecutorKind::PostProcess;
    postAA.features = {"post_aa", "object_id"};

    RenderPlan replacement;
    for (const auto& resource : plan.resources())
      replacement.addResource(resource);
    replacement.addPass(plan.passes().front());
    replacement.addPass(postAA);
    return replacement;
  }

  RenderPlan selectorRoutePlan() {
    RenderIntent intent;
    RenderViewOverride route;
    route.selector = SceneSelector::tag("hero");
    route.viewMode = RenderViewMode::Wireframe;
    intent.viewOverrides.push_back(route);

    RenderSceneAnalysis analysis;
    analysis.recordSelectableObject("sphere-1", "Hero Sphere", {"hero"}, {});

    RenderGraphCompiler compiler;
    return compiler.compile({64, 64, 1}, intent, analysis);
  }

  RenderPlan truncatedSubviewPlan() {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.setMaxRenderToTextureRecursionDepth(0);

    RenderSubviewIntent subview;
    subview.name = "mirror probe";
    subview.view.selector = SceneSelector::all();
    subview.view.executor = RenderExecutorPreference::Rasterizer;
    intent.subviews.push_back(subview);

    return compiler.compile({64, 32, 1}, intent);
  }

  RenderPlan portalAndMirrorPlan() {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.setDefaultCamera(RenderCameraRef{"active-camera", std::nullopt});

    RenderSceneAnalysis analysis;
    analysis.recordPortalReceiverSurface("portal-panel", "Portal Panel",
                                         Matrix4d::translate(2.0, 0.0, 0.0),
                                         Matrix4d::translate(12.0, 0.0, 0.0));
    analysis.recordPlanarMirrorSurface("mirror-panel", "Mirror Panel", Vector3d(0.0, 0.0, 0.0),
                                       Vector3d(0.0, 1.0, 0.0));

    return compiler.compile({64, 32, 1}, intent, analysis);
  }

  std::shared_ptr<render::Camera> camera() {
    return std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);
  }

  std::shared_ptr<render::Scene> highContrastScene() {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord::black());
    auto sphere = std::make_shared<render::Sphere>(Vector3d::null, 1.25);
    sphere->setMaterial(std::make_shared<render::MatteMaterial>(
      std::make_shared<render::ConstantColorTexture>(Colord::white())));
    scene->add(sphere);
    return scene;
  }

  std::shared_ptr<render::Scene> litHighContrastScene() {
    auto scene = highContrastScene();
    scene->addLight(std::make_shared<render::PointLight>(Vector3d(0, 4, -3), Colord::white()));
    return scene;
  }

  std::shared_ptr<render::Scene> diffusePathLoopScene() {
    const Colord background(0.125, 0.25, 0.5);
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(background);
    scene->setEnvironmentRadiance(background);

    auto receiver = std::make_shared<render::Rectangle>(
      Vector3d(-20.0, -20.0, 0.0), Vector3d(40.0, 0.0, 0.0), Vector3d(0.0, 40.0, 0.0));
    receiver->setMaterial(std::make_shared<render::MatteMaterial>(
      std::make_shared<render::ConstantColorTexture>(Colord(0.8, 0.8, 0.8))));
    scene->add(receiver);
    return scene;
  }

  std::shared_ptr<render::Scene> unsupportedExactScene() {
    auto scene = std::make_shared<render::Scene>();
    auto curve =
      std::make_shared<render::Curve>(core::Polyline({Vector3d(0, 0, 0), Vector3d(1, 0, 0)}), 0.1);
    curve->setName("render curve");
    scene->add(curve);
    return scene;
  }

  std::shared_ptr<const RenderGraphExecutionTrace> postProcessTrace() {
    RenderIntent intent;
    intent.postProcessAA = RenderPostProcessAA::FXAA;

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({24, 24, 1}, intent));

    Buffer<unsigned int> buffer(24, 24);
    engine.render(buffer);
    return engine.lastExecutionTrace();
  }

  std::shared_ptr<const RenderGraphExecutionTrace> rasterTrace() {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({24, 24, 1}, intent));

    Buffer<unsigned int> buffer(24, 24);
    engine.render(buffer);
    return engine.lastExecutionTrace();
  }

  std::shared_ptr<const RenderGraphExecutionTrace>
  wavefrontTraceForBackend(const char* intersectionBackend) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Wavefront;
    intent.engineOptions.raytracer().setIntegrator("pathtracer");
    intent.engineOptions.raytracer().setSamplesPerPixel(4);
    intent.engineOptions.raytracer().setIntersectionBackend(intersectionBackend);
    intent.engineOptions.raytracer().setConvergenceEnabled(true);
    intent.engineOptions.raytracer().setConvergenceActiveSampleFractionThreshold(1.0);
    intent.engineOptions.raytracer().setConvergenceRadianceDeltaRmsThreshold(10.0);

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({24, 24, 1}, intent));

    Buffer<unsigned int> buffer(24, 24);
    engine.render(buffer);
    return engine.lastExecutionTrace();
  }

  std::shared_ptr<const RenderGraphExecutionTrace> wavefrontDirectLightGpuTrace() {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Wavefront;
    intent.engineOptions.raytracer().setIntegrator("pathtracer");
    intent.engineOptions.raytracer().setSamplesPerPixel(4);
    intent.engineOptions.raytracer().setIntersectionBackend("gpu");

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), litHighContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({24, 24, 1}, intent));

    Buffer<unsigned int> buffer(24, 24);
    engine.render(buffer);
    return engine.lastExecutionTrace();
  }

  std::shared_ptr<const RenderGraphExecutionTrace> residentPathLoopTrace() {
    auto scene = diffusePathLoopScene();

    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Wavefront;
    intent.engineOptions.raytracer().setIntegrator("pathtracer");
    intent.engineOptions.raytracer().setTracingExecution(TracingExecutionPreference::GPU);
    intent.engineOptions.raytracer().setSamplesPerPixel(1);
    intent.engineOptions.raytracer().setMaximumRecursionDepth(2);
    intent.engineOptions.raytracer().setSampleStreamMode("gpu_sample_stream");

    RenderSceneAnalysis analysis;
    analysis.setFullGpuTracingSupportFromScene(*scene);

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), scene);
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({8, 8, 1}, intent, analysis));

    Buffer<unsigned int> buffer(8, 8);
    engine.render(buffer);
    return engine.lastExecutionTrace();
  }

  std::shared_ptr<const RenderGraphExecutionTrace> hybridVisibilityTrace() {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Wavefront;
    intent.defaultViewMode = RenderViewMode::HybridVisibility;
    intent.engineOptions.raytracer().setIntersectionBackend("cpu");

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({24, 24, 1}, intent));

    Buffer<unsigned int> buffer(24, 24);
    engine.render(buffer);
    return engine.lastExecutionTrace();
  }

  std::shared_ptr<const RenderGraphExecutionTrace> unsupportedHybridVisibilityTrace() {
    auto scene = std::make_shared<render::Scene>();
    auto curve =
      std::make_shared<render::Curve>(core::Polyline({Vector3d(-1, 0, 0), Vector3d(1, 0, 0)}), 0.1);
    curve->setName("debug curve");
    scene->add(curve);

    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Wavefront;
    intent.defaultViewMode = RenderViewMode::HybridVisibility;
    intent.engineOptions.raytracer().setIntersectionBackend("gpu");

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), scene);
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({12, 12, 1}, intent));

    Buffer<unsigned int> buffer(12, 12);
    engine.render(buffer);
    return engine.lastExecutionTrace();
  }

  std::shared_ptr<const RenderGraphExecutionTrace> hybridRayTracedShadowTrace() {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.enablePreviewShadows = true;
    intent.engineOptions.rasterizer().setShadowMode(RenderRasterShadowMode::RayTraced);
    intent.engineOptions.raytracer().setIntersectionBackend("cpu");

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), litHighContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({24, 24, 1}, intent));

    Buffer<unsigned int> buffer(24, 24);
    engine.render(buffer);
    return engine.lastExecutionTrace();
  }

  std::shared_ptr<const RenderGraphExecutionTrace> unsupportedWavefrontGpuTrace() {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Wavefront;
    intent.engineOptions.raytracer().setIntegrator("pathtracer");
    intent.engineOptions.raytracer().setSamplesPerPixel(1);
    intent.engineOptions.raytracer().setIntersectionBackend("gpu");

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), unsupportedExactScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({12, 12, 1}, intent));

    Buffer<unsigned int> buffer(12, 12);
    engine.render(buffer);
    return engine.lastExecutionTrace();
  }

  std::shared_ptr<const RenderGraphExecutionTrace> wavefrontCpuTrace() {
    return wavefrontTraceForBackend("cpu");
  }

  std::shared_ptr<const RenderGraphExecutionTrace> wavefrontGpuTrace() {
    return wavefrontTraceForBackend("gpu");
  }

  std::shared_ptr<const RenderGraphExecutionTrace> wavefrontAutoTrace() {
    return wavefrontTraceForBackend("auto");
  }

  void processEventsFor(int milliseconds) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
      QApplication::processEvents(QEventLoop::AllEvents, 20);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    QApplication::processEvents(QEventLoop::AllEvents, 20);
  }

  void selectPass(RenderGraphInspectorWidget& widget, const QString& passId) {
    auto* passes = widget.findChild<QTreeWidget*>("renderGraphPasses");
    ASSERT_NE(nullptr, passes);
    for (int row = 0; row != passes->topLevelItemCount(); ++row) {
      QTreeWidgetItem* item = passes->topLevelItem(row);
      if (item->data(0, Qt::UserRole).toString() == passId) {
        passes->setCurrentItem(item);
        return;
      }
    }
    FAIL() << "missing pass row " << passId.toStdString();
  }

  void selectResource(RenderGraphInspectorWidget& widget, const QString& resourceId) {
    auto* resources = widget.findChild<QTreeWidget*>("renderGraphResources");
    ASSERT_NE(nullptr, resources);
    for (int row = 0; row != resources->topLevelItemCount(); ++row) {
      QTreeWidgetItem* item = resources->topLevelItem(row);
      if (item->data(0, Qt::UserRole).toString() == resourceId) {
        resources->setCurrentItem(item);
        return;
      }
    }
    FAIL() << "missing resource row " << resourceId.toStdString();
  }

  QTreeWidgetItem* passItem(RenderGraphInspectorWidget& widget, const QString& passId) {
    auto* passes = widget.findChild<QTreeWidget*>("renderGraphPasses");
    if (!passes)
      return nullptr;
    for (int row = 0; row != passes->topLevelItemCount(); ++row) {
      QTreeWidgetItem* item = passes->topLevelItem(row);
      if (item->data(0, Qt::UserRole).toString() == passId)
        return item;
    }
    return nullptr;
  }

  QTreeWidgetItem* groupItem(RenderGraphInspectorWidget& widget, const QString& scope,
                             const QString& value) {
    auto* groups = widget.findChild<QTreeWidget*>("renderGraphGroups");
    if (!groups)
      return nullptr;
    for (int row = 0; row != groups->topLevelItemCount(); ++row) {
      QTreeWidgetItem* item = groups->topLevelItem(row);
      if (item->text(1) == scope && item->text(2) == value)
        return item;
    }
    return nullptr;
  }

  QString rowValue(const RenderGraphInspectorWidget::DetailRows& rows, const QString& name) {
    for (const auto& row : rows) {
      if (row.first == name)
        return row.second;
    }
    return QString();
  }

  int rowIndex(const RenderGraphInspectorWidget::DetailRows& rows, const QString& name) {
    for (int index = 0; index != rows.size(); ++index) {
      if (rows[index].first == name)
        return index;
    }
    return -1;
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldInitialize) {
    RenderGraphInspectorWidget widget;
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowPassAndResourceRows) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(simplePlan());

    auto* passes = widget.findChild<QTreeWidget*>("renderGraphPasses");
    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    auto* resources = widget.findChild<QTreeWidget*>("renderGraphResources");
    auto* status = widget.findChild<QLabel*>("renderGraphValidationStatus");
    ASSERT_NE(nullptr, passes);
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, resources);
    ASSERT_NE(nullptr, status);
    EXPECT_EQ(nullptr, widget.findChild<QTreeWidget*>("renderGraphDependencies"));

    ASSERT_EQ(1, passes->topLevelItemCount());
    EXPECT_EQ(QString("1"), passes->topLevelItem(0)->text(1));
    EXPECT_EQ(QString("1"), passes->topLevelItem(0)->text(2));
    EXPECT_EQ(QString("Raytraced beauty"), passes->topLevelItem(0)->text(3));
    EXPECT_EQ(QString("raytrace_beauty"), passes->topLevelItem(0)->toolTip(3));
    EXPECT_EQ(QString(), passes->topLevelItem(0)->text(4));
    EXPECT_EQ(QString("Beauty"), passes->topLevelItem(0)->text(5));
    EXPECT_EQ(QString("Raytracer"), passes->topLevelItem(0)->text(6));
    EXPECT_EQ(QString("all"), passes->topLevelItem(0)->text(7));
    EXPECT_EQ(QString("preview-camera"), passes->topLevelItem(0)->text(8));
    EXPECT_EQ(QString("clay"), passes->topLevelItem(0)->text(9));
    EXPECT_EQ(QString("Main color"), passes->topLevelItem(0)->text(11));

    ASSERT_NE(nullptr, graph->scene());
    auto* passNode = graphNodeItem(graph->scene(), "pass", "raytrace_beauty");
    ASSERT_NE(nullptr, passNode);
    EXPECT_TRUE(nodeTextContains(passNode, "Raytraced beauty"));
    EXPECT_TRUE(nodeTextContains(passNode, "camera preview-camera"));
    EXPECT_TRUE(nodeTextContains(passNode, "shading clay"));
    EXPECT_TRUE(nodeTextContains(passNode, "stage 1"));
    EXPECT_TRUE(passNode->toolTip().contains("Scene selector: all"));
    EXPECT_TRUE(passNode->toolTip().contains("Scene camera: preview-camera"));
    EXPECT_TRUE(passNode->toolTip().contains("Shading profile: clay"));
    EXPECT_TRUE(passNode->toolTip().contains("Pass ID: raytrace_beauty"));
    EXPECT_TRUE(passNode->toolTip().contains("Reads: -"));
    EXPECT_TRUE(passNode->toolTip().contains("Writes: Main color"));
    EXPECT_TRUE(passNode->toolTip().contains("Incoming dependencies: -"));
    auto* resourceNode = graphNodeItem(graph->scene(), "resource", "main_color");
    ASSERT_NE(nullptr, resourceNode);
    EXPECT_TRUE(nodeTextContains(resourceNode, "Main color"));
    EXPECT_TRUE(nodeTextContains(resourceNode, "RGB double"));
    EXPECT_TRUE(nodeTextContains(resourceNode, "Exported"));
    EXPECT_TRUE(resourceNode->toolTip().contains("Resource ID: main_color"));
    EXPECT_TRUE(resourceNode->toolTip().contains("Producer: Raytraced beauty"));
    EXPECT_TRUE(resourceNode->toolTip().contains("Consumers: -"));

    ASSERT_EQ(1, resources->topLevelItemCount());
    EXPECT_EQ(QString("Main color"), resources->topLevelItem(0)->text(0));
    EXPECT_EQ(QString("main_color"), resources->topLevelItem(0)->toolTip(0));
    EXPECT_EQ(QString("Raytraced beauty"), resources->topLevelItem(0)->text(1));
    EXPECT_EQ(QString("-"), resources->topLevelItem(0)->text(2));
    EXPECT_EQ(QString("Color"), resources->topLevelItem(0)->text(3));
    EXPECT_THAT(status->text().toStdString(), ::testing::HasSubstr("Valid plan"));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowCompileErrorState) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(simplePlan());

    widget.setError(QStringLiteral("selector-specific render intent"));

    auto* passes = widget.findChild<QTreeWidget*>("renderGraphPasses");
    auto* resources = widget.findChild<QTreeWidget*>("renderGraphResources");
    auto* status = widget.findChild<QLabel*>("renderGraphValidationStatus");
    ASSERT_NE(nullptr, passes);
    ASSERT_NE(nullptr, resources);
    ASSERT_NE(nullptr, status);
    EXPECT_EQ(0, passes->topLevelItemCount());
    EXPECT_EQ(0, resources->topLevelItemCount());
    EXPECT_FALSE(widget.effectivePlanValid());
    EXPECT_TRUE(status->text().contains("Graph compile failed"));
    EXPECT_TRUE(status->text().contains("selector-specific render intent"));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldDisablePassesByGroupOverride) {
    RenderGraphInspectorWidget widget;
    Slot slot;
    QObject::connect(&widget, SIGNAL(overridesChanged()), &slot, SLOT(receive()));
    widget.setPlan(twoPassPlan());

    QTreeWidgetItem* tonemapKind =
      groupItem(widget, QStringLiteral("Kind"), QStringLiteral("Tone map"));
    ASSERT_NE(nullptr, tonemapKind);
    tonemapKind->setCheckState(0, Qt::Unchecked);

    const RenderPlan effective = widget.effectivePlan();
    const auto* tonemap = effective.findPass("tonemap");
    ASSERT_NE(nullptr, tonemap);
    EXPECT_FALSE(tonemap->enabled);
    EXPECT_TRUE(slot.called());

    const auto overrides = widget.overrides();
    EXPECT_TRUE(overrides.disabledPassKinds.count(RenderPassKind::Tonemap));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldHumanizeFeatureGroupsAndKeepRawOverrideValue) {
    RenderGraphInspectorWidget widget;
    Slot slot;
    QObject::connect(&widget, SIGNAL(overridesChanged()), &slot, SLOT(receive()));
    widget.setPlan(featureGroupPlan());

    QTreeWidgetItem* postAA =
      groupItem(widget, QStringLiteral("Feature"), QStringLiteral("Post AA"));
    ASSERT_NE(nullptr, postAA);
    EXPECT_EQ(QStringLiteral("post_aa"), postAA->toolTip(2));

    QTreeWidgetItem* objectId =
      groupItem(widget, QStringLiteral("Feature"), QStringLiteral("Object ID"));
    ASSERT_NE(nullptr, objectId);
    EXPECT_EQ(QStringLiteral("object_id"), objectId->toolTip(2));

    postAA->setCheckState(0, Qt::Unchecked);

    const RenderPlan effective = widget.effectivePlan();
    const auto* pass = effective.findPass("post_fxaa");
    ASSERT_NE(nullptr, pass);
    EXPECT_FALSE(pass->enabled);
    EXPECT_TRUE(slot.called());

    const auto overrides = widget.overrides();
    EXPECT_TRUE(overrides.disabledFeatures.count("post_aa"));
    EXPECT_FALSE(overrides.disabledFeatures.count("Post AA"));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldLabelSelectorRouteBranches) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(selectorRoutePlan());

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());

    QGraphicsItem* branch = graphNodeItem(graph->scene(), "pass", "selector_1_wireframe_beauty");
    ASSERT_NE(nullptr, branch);
    EXPECT_TRUE(nodeTextContains(branch, QStringLiteral("routed selector")));
    EXPECT_TRUE(nodeTextContains(branch, QStringLiteral("selector tag: hero")));
    EXPECT_TRUE(branch->toolTip().contains("Selector route: compiler-generated branch"));
    EXPECT_TRUE(branch->toolTip().contains("tag: hero"));

    QGraphicsItem* resource =
      graphNodeItem(graph->scene(), "resource", "selector_1_composited_color");
    ASSERT_NE(nullptr, resource);
    EXPECT_TRUE(nodeTextContains(resource, QStringLiteral("routed selector")));
    EXPECT_TRUE(resource->toolTip().contains("Selector route: compiler-generated branch resource"));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowTruncatedSubviewDiagnostics) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(truncatedSubviewPlan());

    auto* passes = widget.findChild<QTreeWidget*>("renderGraphPasses");
    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, passes);
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());

    QTreeWidgetItem* diagnosticRow = nullptr;
    for (int row = 0; row != passes->topLevelItemCount(); ++row) {
      if (passes->topLevelItem(row)->data(0, Qt::UserRole).toString() ==
          QStringLiteral("subview_mirror_probe_recursion_limit")) {
        diagnosticRow = passes->topLevelItem(row);
        break;
      }
    }

    ASSERT_NE(nullptr, diagnosticRow);
    EXPECT_EQ(Qt::Unchecked, diagnosticRow->checkState(0));
    EXPECT_EQ(
      QStringLiteral("Subview mirror probe truncated at render-to-texture recursion limit 0"),
      diagnosticRow->text(3));
    EXPECT_EQ(QStringLiteral("Debug"), diagnosticRow->text(5));

    QGraphicsItem* diagnosticNode =
      graphNodeItem(graph->scene(), "pass", "subview_mirror_probe_recursion_limit");
    ASSERT_NE(nullptr, diagnosticNode);
    EXPECT_TRUE(nodeLineTooltipContains(diagnosticNode, QStringLiteral("truncated")));
    EXPECT_TRUE(diagnosticNode->toolTip().contains(QStringLiteral("recursion_limit")));

    QTreeWidgetItem* diagnosticFeature = groupItem(
      widget, QStringLiteral("Feature"), QStringLiteral("Render To Texture Recursion Limit"));
    ASSERT_NE(nullptr, diagnosticFeature);
    EXPECT_EQ(QStringLiteral("render_to_texture_recursion_limit"), diagnosticFeature->toolTip(2));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowPortalAndMirrorSynthesizedGraphNodes) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(portalAndMirrorPlan());

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());

    QTreeWidgetItem* portalSubview =
      passItem(widget, QStringLiteral("subview_portal_portal_panel_raytrace_beauty"));
    ASSERT_NE(nullptr, portalSubview);
    EXPECT_EQ(QStringLiteral("Subview portal Portal Panel Raytraced beauty"),
              portalSubview->text(3));
    EXPECT_TRUE(portalSubview->text(8).contains(QStringLiteral("derived portal")));
    EXPECT_TRUE(portalSubview->text(8).contains(QStringLiteral("active-camera")));
    EXPECT_TRUE(portalSubview->text(8).contains(QStringLiteral("clipped")));
    EXPECT_TRUE(portalSubview->text(10).contains(QStringLiteral("Portal Panel receiver mask")));

    QTreeWidgetItem* portalMask =
      passItem(widget, QStringLiteral("subview_portal_portal_panel_receiver_mask"));
    ASSERT_NE(nullptr, portalMask);
    EXPECT_EQ(QStringLiteral("object_id: portal-panel"), portalMask->text(7));
    EXPECT_EQ(QStringLiteral("AOV"), portalMask->text(5));
    EXPECT_EQ(QStringLiteral("Rasterizer"), portalMask->text(6));

    QTreeWidgetItem* portalComposite =
      passItem(widget, QStringLiteral("subview_portal_portal_panel_composite"));
    ASSERT_NE(nullptr, portalComposite);
    EXPECT_EQ(QStringLiteral("Composite"), portalComposite->text(5));
    EXPECT_EQ(QStringLiteral("Composite"), portalComposite->text(6));
    EXPECT_TRUE(portalComposite->text(10).contains(QStringLiteral("Beauty color")));
    EXPECT_TRUE(
      portalComposite->text(10).contains(QStringLiteral("Subview portal Portal Panel Main color")));
    EXPECT_TRUE(portalComposite->text(10).contains(QStringLiteral("Portal Panel receiver mask")));

    QTreeWidgetItem* mirrorSubview =
      passItem(widget, QStringLiteral("subview_mirror_mirror_panel_raytrace_beauty"));
    ASSERT_NE(nullptr, mirrorSubview);
    EXPECT_TRUE(mirrorSubview->text(8).contains(QStringLiteral("derived planar_mirror")));
    EXPECT_TRUE(mirrorSubview->text(8).contains(QStringLiteral("active-camera")));
    EXPECT_TRUE(mirrorSubview->text(10).contains(QStringLiteral("Mirror Panel receiver mask")));

    QTreeWidgetItem* mirrorMask =
      passItem(widget, QStringLiteral("subview_mirror_mirror_panel_receiver_mask"));
    ASSERT_NE(nullptr, mirrorMask);
    EXPECT_EQ(QStringLiteral("object_id: mirror-panel"), mirrorMask->text(7));

    QTreeWidgetItem* mirrorComposite =
      passItem(widget, QStringLiteral("subview_mirror_mirror_panel_composite"));
    ASSERT_NE(nullptr, mirrorComposite);
    EXPECT_EQ(QStringLiteral("Composite"), mirrorComposite->text(5));
    EXPECT_TRUE(
      mirrorComposite->text(10).contains(QStringLiteral("Portal Panel composited color")));
    EXPECT_TRUE(
      mirrorComposite->text(10).contains(QStringLiteral("Subview mirror Mirror Panel Main color")));
    EXPECT_TRUE(mirrorComposite->text(10).contains(QStringLiteral("Mirror Panel receiver mask")));

    QGraphicsItem* portalSubviewNode =
      graphNodeItem(graph->scene(), "pass", "subview_portal_portal_panel_raytrace_beauty");
    ASSERT_NE(nullptr, portalSubviewNode);
    EXPECT_TRUE(nodeTextContains(portalSubviewNode, QStringLiteral("derived portal")));
    EXPECT_TRUE(portalSubviewNode->toolTip().contains(
      QStringLiteral("Scene camera: derived portal from active-camera, clipped")));

    EXPECT_NE(nullptr,
              graphNodeItem(graph->scene(), "pass", "subview_portal_portal_panel_receiver_mask"));
    EXPECT_NE(nullptr,
              graphNodeItem(graph->scene(), "pass", "subview_portal_portal_panel_composite"));
    EXPECT_NE(nullptr,
              graphNodeItem(graph->scene(), "pass", "subview_mirror_mirror_panel_raytrace_beauty"));
    EXPECT_NE(nullptr,
              graphNodeItem(graph->scene(), "pass", "subview_mirror_mirror_panel_receiver_mask"));
    EXPECT_NE(nullptr,
              graphNodeItem(graph->scene(), "pass", "subview_mirror_mirror_panel_composite"));

    QTreeWidgetItem* subviewFeature =
      groupItem(widget, QStringLiteral("Feature"), QStringLiteral("Subview"));
    ASSERT_NE(nullptr, subviewFeature);
    EXPECT_EQ(QStringLiteral("subview"), subviewFeature->toolTip(2));
    QTreeWidgetItem* compositeFeature =
      groupItem(widget, QStringLiteral("Feature"), QStringLiteral("Subview Composite"));
    ASSERT_NE(nullptr, compositeFeature);
    EXPECT_EQ(QStringLiteral("subview_composite"), compositeFeature->toolTip(2));
    QTreeWidgetItem* maskFeature =
      groupItem(widget, QStringLiteral("Feature"), QStringLiteral("Receiver Mask"));
    ASSERT_NE(nullptr, maskFeature);
    EXPECT_EQ(QStringLiteral("receiver_mask"), maskFeature->toolTip(2));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowResourceEdgeRows) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(twoPassPlan());

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    EXPECT_EQ(nullptr, widget.findChild<QTreeWidget*>("renderGraphDependencies"));

    auto* resources = widget.findChild<QTreeWidget*>("renderGraphResources");
    ASSERT_NE(nullptr, resources);
    ASSERT_EQ(2, resources->topLevelItemCount());
    EXPECT_EQ(QString("Beauty color"), resources->topLevelItem(0)->text(0));
    EXPECT_EQ(QString("Raytraced beauty"), resources->topLevelItem(0)->text(1));
    EXPECT_EQ(QString("Tone map"), resources->topLevelItem(0)->text(2));
    EXPECT_NE(nullptr, graphItem(graph->scene(), "pass", "raytrace_beauty"));
    EXPECT_NE(nullptr, graphItem(graph->scene(), "pass", "tonemap"));
    EXPECT_NE(nullptr, graphItem(graph->scene(), "resource", "beauty_color"));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowRenderToTextureScreenBranch) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(renderTextureScreenPlan());

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    auto* resources = widget.findChild<QTreeWidget*>("renderGraphResources");
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, resources);

    auto* subviewPass = graphNodeItem(graph->scene(), "pass", "subview_monitor_feed_raster_beauty");
    auto* subviewColor =
      graphNodeItem(graph->scene(), "resource", "subview_monitor_feed_main_color");
    auto* subviewDepth =
      graphNodeItem(graph->scene(), "resource", "subview_monitor_feed_depth_aov");
    auto* finalBeauty = graphNodeItem(graph->scene(), "pass", "raytrace_beauty");
    ASSERT_NE(nullptr, subviewPass);
    ASSERT_NE(nullptr, subviewColor);
    ASSERT_NE(nullptr, subviewDepth);
    ASSERT_NE(nullptr, finalBeauty);

    EXPECT_TRUE(subviewPass->toolTip().contains("Pass ID: subview_monitor_feed_raster_beauty"));
    EXPECT_TRUE(subviewColor->toolTip().contains("Resource ID: subview_monitor_feed_main_color"));
    EXPECT_TRUE(subviewDepth->toolTip().contains("Resource ID: subview_monitor_feed_depth_aov"));
    EXPECT_TRUE(finalBeauty->toolTip().contains("Reads: Monitor feed main color"));
    EXPECT_TRUE(finalBeauty->toolTip().contains("Monitor feed depth AOV"));
    EXPECT_TRUE(
      finalBeauty->toolTip().contains("Incoming dependencies: Monitor feed Raster beauty"));
    EXPECT_TRUE(finalBeauty->toolTip().contains("Monitor feed Depth AOV"));

    bool sawSubviewColorRow = false;
    bool sawSubviewDepthRow = false;
    for (int row = 0; row != resources->topLevelItemCount(); ++row) {
      QTreeWidgetItem* item = resources->topLevelItem(row);
      if (item->toolTip(0) == QStringLiteral("subview_monitor_feed_main_color")) {
        sawSubviewColorRow = true;
        EXPECT_EQ(QString("Monitor feed Raster beauty"), item->text(1));
        EXPECT_EQ(QString("Raytraced beauty"), item->text(2));
      } else if (item->toolTip(0) == QStringLiteral("subview_monitor_feed_depth_aov")) {
        sawSubviewDepthRow = true;
        EXPECT_EQ(QString("Monitor feed Depth AOV"), item->text(1));
        EXPECT_EQ(QString("Raytraced beauty"), item->text(2));
      }
    }
    EXPECT_TRUE(sawSubviewColorRow);
    EXPECT_TRUE(sawSubviewDepthRow);
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldPlaceResourceBetweenConnectedPassNodes) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(twoPassPlan());

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    QGraphicsItem* beauty = graphItem(graph->scene(), "pass", "raytrace_beauty");
    QGraphicsItem* resource = graphItem(graph->scene(), "resource", "beauty_color");
    QGraphicsItem* tonemap = graphItem(graph->scene(), "pass", "tonemap");
    ASSERT_NE(nullptr, beauty);
    ASSERT_NE(nullptr, resource);
    ASSERT_NE(nullptr, tonemap);

    EXPECT_GT(resource->sceneBoundingRect().left(), beauty->sceneBoundingRect().right());
    EXPECT_LT(resource->sceneBoundingRect().right(), tonemap->sceneBoundingRect().left());
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldTogglePassFromGraphNode) {
    RenderGraphInspectorWidget widget;
    Slot slot;
    QObject::connect(&widget, SIGNAL(overridesChanged()), &slot, SLOT(receive()));
    widget.setPlan(twoPassPlan());

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());
    QGraphicsItem* tonemap = graphItem(graph->scene(), "pass", "tonemap");
    ASSERT_NE(nullptr, tonemap);

    QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMouseDoubleClick);
    event.setScenePos(tonemap->sceneBoundingRect().center());
    QApplication::sendEvent(graph->scene(), &event);

    const auto overrides = widget.overrides();
    EXPECT_TRUE(slot.called());
    EXPECT_NE(overrides.disabledPasses.end(), overrides.disabledPasses.find("tonemap"));
    auto* passes = widget.findChild<QTreeWidget*>("renderGraphPasses");
    ASSERT_NE(nullptr, passes);
    ASSERT_EQ(2, passes->topLevelItemCount());
    EXPECT_EQ(Qt::Unchecked, passes->topLevelItem(1)->checkState(0));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldMarkRequiredGraphNodeToggleInvalid) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(twoPassPlan());

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());
    QGraphicsItem* beauty = graphItem(graph->scene(), "pass", "raytrace_beauty");
    ASSERT_NE(nullptr, beauty);

    QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMouseDoubleClick);
    event.setScenePos(beauty->sceneBoundingRect().center());
    QApplication::sendEvent(graph->scene(), &event);

    const auto overrides = widget.overrides();
    EXPECT_NE(overrides.disabledPasses.end(), overrides.disabledPasses.find("raytrace_beauty"));
    EXPECT_FALSE(widget.effectivePlanValid());

    auto* status = widget.findChild<QLabel*>("renderGraphValidationStatus");
    ASSERT_NE(nullptr, status);
    EXPECT_THAT(status->text().toStdString(), ::testing::HasSubstr("Invalid plan"));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldDelayLiveExecutionStateOnGraphNodes) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(twoPassPlan());

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);

    widget.passExecutionStarted(QStringLiteral("raytrace_beauty"));
    QGraphicsItem* immediate = graphNodeItem(graph->scene(), "pass", "raytrace_beauty");
    ASSERT_NE(nullptr, immediate);
    EXPECT_EQ(QString("idle"), immediate->data(2).toString());

    processEventsFor(560);
    QGraphicsItem* running = graphNodeItem(graph->scene(), "pass", "raytrace_beauty");
    ASSERT_NE(nullptr, running);
    EXPECT_EQ(QString("running"), running->data(2).toString());

    widget.passExecutionFinished(QStringLiteral("raytrace_beauty"));
    QGraphicsItem* completed = graphNodeItem(graph->scene(), "pass", "raytrace_beauty");
    ASSERT_NE(nullptr, completed);
    EXPECT_EQ(QString("completed"), completed->data(2).toString());

    widget.clearExecutionState();
    QGraphicsItem* idle = graphNodeItem(graph->scene(), "pass", "raytrace_beauty");
    ASSERT_NE(nullptr, idle);
    EXPECT_EQ(QString("idle"), idle->data(2).toString());
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldSuppressShortLiveExecutionStateOnGraphNodes) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(twoPassPlan());

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);

    widget.passExecutionStarted(QStringLiteral("raytrace_beauty"));
    widget.passExecutionFinished(QStringLiteral("raytrace_beauty"));
    processEventsFor(560);

    QGraphicsItem* idle = graphNodeItem(graph->scene(), "pass", "raytrace_beauty");
    ASSERT_NE(nullptr, idle);
    EXPECT_EQ(QString("idle"), idle->data(2).toString());
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldHighlightMultipleActiveGraphNodesTogether) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(twoPassPlan());

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);

    widget.setActiveExecutionPasses({QStringLiteral("raytrace_beauty"), QStringLiteral("tonemap")});
    processEventsFor(560);

    QGraphicsItem* beauty = graphNodeItem(graph->scene(), "pass", "raytrace_beauty");
    QGraphicsItem* tonemap = graphNodeItem(graph->scene(), "pass", "tonemap");
    ASSERT_NE(nullptr, beauty);
    ASSERT_NE(nullptr, tonemap);
    EXPECT_EQ(QString("running"), beauty->data(2).toString());
    EXPECT_EQ(QString("running"), tonemap->data(2).toString());

    widget.setActiveExecutionPasses({});
    processEventsFor(60);

    beauty = graphNodeItem(graph->scene(), "pass", "raytrace_beauty");
    tonemap = graphNodeItem(graph->scene(), "pass", "tonemap");
    ASSERT_NE(nullptr, beauty);
    ASSERT_NE(nullptr, tonemap);
    EXPECT_EQ(QString("idle"), beauty->data(2).toString());
    EXPECT_EQ(QString("idle"), tonemap->data(2).toString());
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowFailedExecutionStateOnGraphNodes) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(twoPassPlan());

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);

    widget.passExecutionFailed(QStringLiteral("tonemap"), QStringLiteral("boom"));

    QGraphicsItem* failed = graphNodeItem(graph->scene(), "pass", "tonemap");
    ASSERT_NE(nullptr, failed);
    EXPECT_EQ(QString("failed"), failed->data(2).toString());
    EXPECT_THAT(failed->toolTip().toStdString(), ::testing::HasSubstr("boom"));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldNotExposeTraceTab) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(twoPassPlan());

    auto* tabs = widget.findChild<QTabWidget*>();
    ASSERT_NE(nullptr, tabs);
    for (int index = 0; index != tabs->count(); ++index) {
      EXPECT_NE(QString("Trace"), tabs->tabText(index));
    }
    EXPECT_EQ(nullptr, widget.findChild<QWidget*>("renderGraphTraceInputs"));
    EXPECT_EQ(nullptr, widget.findChild<QTreeWidget*>("renderGraphTraceMetadata"));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldEmitGraphExportData) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(twoPassPlan());

    QString format;
    QByteArray data;
    QObject::connect(&widget, &RenderGraphInspectorWidget::graphExportRequested,
                     [&](const QString& emittedFormat, const QByteArray& emittedData) {
                       format = emittedFormat;
                       data = emittedData;
                     });

    auto* json = widget.findChild<QToolButton*>("renderGraphExportJson");
    ASSERT_NE(nullptr, json);
    json->click();

    EXPECT_EQ(QStringLiteral("json"), format);
    EXPECT_TRUE(data.contains("\"passes\""));
    EXPECT_TRUE(data.contains("raytrace_beauty"));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowTraceSummaryOnGraphNodes) {
    auto trace = postProcessTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());

    QGraphicsItem* pass = graphNodeItem(graph->scene(), "pass", "post_fxaa");
    QGraphicsItem* resource = graphNodeItem(graph->scene(), "resource", "post_aa_color");
    ASSERT_NE(nullptr, pass);
    ASSERT_NE(nullptr, resource);

    EXPECT_TRUE(nodeTextContains(pass, QStringLiteral("completed")));
    EXPECT_TRUE(nodeTextContains(resource, QStringLiteral("trace: color")));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldExposeWavefrontPacketWidthSummaryOnGraphNode) {
    auto trace = wavefrontCpuTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());

    QGraphicsItem* pass = graphNodeItem(graph->scene(), "pass", "wavefront_beauty");
    ASSERT_NE(nullptr, pass);

    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("packets")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("Ray8")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("Ray4")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("fill")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("scalar tail")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("expected")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("intersection rays")));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldGroupCpuTracingExecutionRowsForSelectedPass) {
    auto trace = wavefrontCpuTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);

    const auto rows = widget.passDetailRows(QStringLiteral("wavefront_beauty"));

    EXPECT_EQ(QStringLiteral("CPU"), rowValue(rows, QStringLiteral("Tracing backend")));
    EXPECT_EQ(QStringLiteral("Wavefront Intersection"),
              rowValue(rows, QStringLiteral("Tracing backend mode")));
    const QString cpuExecution = rowValue(rows, QStringLiteral("CPU execution"));
    EXPECT_THAT(cpuExecution.toStdString(), ::testing::HasSubstr("Geometry Closest Hit"));
    EXPECT_THAT(cpuExecution.toStdString(), ::testing::HasSubstr("Shading BSDF Eval"));
    EXPECT_EQ(QStringLiteral("none"), rowValue(rows, QStringLiteral("Hybrid execution")));
    EXPECT_EQ(QStringLiteral("none"), rowValue(rows, QStringLiteral("GPU execution")));
    EXPECT_EQ(QStringLiteral("none"), rowValue(rows, QStringLiteral("Tracing fallback")));
    EXPECT_EQ(QStringLiteral("none"), rowValue(rows, QStringLiteral("Fallback capabilities")));
    EXPECT_EQ(QStringLiteral("none"), rowValue(rows, QStringLiteral("Restricted capabilities")));
    EXPECT_THAT(rowValue(rows, QStringLiteral("Unsupported capabilities")).toStdString(),
                ::testing::HasSubstr("Sampling GPU RNG"));
    EXPECT_EQ(QStringLiteral("CPU"), rowValue(rows, QStringLiteral("Actual tracing execution")));
    EXPECT_EQ(QStringLiteral("none"), rowValue(rows, QStringLiteral("Actual tracing fallback")));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowPredictedTracingExecutionBeforeTrace) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    intent.engineOptions.raytracer().setIntegrator("pathtracer");
    intent.engineOptions.raytracer().setTracingExecution(TracingExecutionPreference::GPU);

    RenderSceneAnalysis analysis;
    analysis.setFullGpuTracingSupported(false, "transparent material requires CPU shading");
    RenderGraphCompiler compiler;

    RenderGraphInspectorWidget widget;
    widget.setPlan(compiler.compile({24, 24, 1}, intent, analysis));

    const auto rows = widget.passDetailRows(QStringLiteral("wavefront_beauty"));

    EXPECT_EQ(QStringLiteral("GPU"), rowValue(rows, QStringLiteral("Requested tracing execution")));
    EXPECT_EQ(QStringLiteral("Hybrid"),
              rowValue(rows, QStringLiteral("Predicted tracing execution")));
    EXPECT_EQ(QStringLiteral("full GPU tracing backend is not available"),
              rowValue(rows, QStringLiteral("Predicted tracing fallback")));
    EXPECT_EQ(QStringLiteral("not available"), rowValue(rows, QStringLiteral("Trace")));
    EXPECT_TRUE(rowValue(rows, QStringLiteral("Actual tracing execution")).isEmpty());
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowAutoPredictedTracingExecutionBeforeTrace) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    intent.engineOptions.raytracer().setIntegrator("pathtracer");
    intent.engineOptions.raytracer().setSampleStreamMode("gpu_sample_stream");

    RenderSceneAnalysis analysis;
    analysis.setFullGpuTracingSupported(true);
    analysis.setFullGpuTracingBackendAvailable(true);
    RenderGraphCompiler compiler;

    RenderGraphInspectorWidget widget;
    widget.setPlan(compiler.compile({24, 24, 1}, intent, analysis));

    const auto rows = widget.passDetailRows(QStringLiteral("wavefront_beauty"));

    EXPECT_EQ(QStringLiteral("Auto"),
              rowValue(rows, QStringLiteral("Requested tracing execution")));
    EXPECT_EQ(QStringLiteral("GPU"), rowValue(rows, QStringLiteral("Predicted tracing execution")));
    EXPECT_EQ(QStringLiteral("none"), rowValue(rows, QStringLiteral("Predicted tracing fallback")));
    EXPECT_TRUE(rowValue(rows, QStringLiteral("Actual tracing execution")).isEmpty());
  }

  TEST_F(RenderGraphInspectorWidgetTest,
         ShouldExposeWavefrontIntersectionBackendRowsForSelectedPass) {
    auto trace = wavefrontGpuTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);

    const auto rows = widget.passDetailRows(QStringLiteral("wavefront_beauty"));

    EXPECT_EQ(QStringLiteral("Wavefront beauty"), rowValue(rows, QStringLiteral("Name")));
    const int tracingBackendRow = rowIndex(rows, QStringLiteral("Tracing backend"));
    const int intersectionBackendRequestRow =
      rowIndex(rows, QStringLiteral("Intersection backend request"));
    ASSERT_NE(-1, tracingBackendRow);
    ASSERT_NE(-1, intersectionBackendRequestRow);
    EXPECT_LT(tracingBackendRow, intersectionBackendRequestRow);
    EXPECT_FALSE(rowValue(rows, QStringLiteral("CPU execution")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Hybrid execution")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("GPU execution")).isEmpty());
    const QString tracingFallback = rowValue(rows, QStringLiteral("Tracing fallback"));
    EXPECT_FALSE(tracingFallback.isEmpty());
    EXPECT_NE(QStringLiteral("none"), tracingFallback);
    EXPECT_THAT(tracingFallback.toStdString(), ::testing::HasSubstr("GPU -> CPU"));
    EXPECT_THAT(tracingFallback.toStdString(),
                ::testing::HasSubstr("wavefront intersection backend"));
    EXPECT_EQ(QStringLiteral("GPU"),
              rowValue(rows, QStringLiteral("Intersection backend request")));
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection backend")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection backend availability")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection backend execution path")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Closest-hit frontier residency")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Closest-hit frontier packed ray bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Any-hit frontier residency")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Any-hit frontier packed ray bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection backend fallback")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Expected intersection rays")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Expected closest-hit rays")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Expected any-hit rays")).isEmpty());
    EXPECT_TRUE(rowValue(rows, QStringLiteral("Auto minimum GPU rays")).isEmpty());
    EXPECT_EQ(QStringLiteral("yes"), rowValue(rows, QStringLiteral("Intersection scene compiled")));
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection scene primitives")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection scene BVH nodes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection scene upload bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection ray upload bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Closest-hit ray upload bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Any-hit ray upload bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Closest-hit readback bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Any-hit readback bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection query transfer bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Closest-hit query transfer bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Any-hit query transfer bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection query round trips")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Closest-hit query round trips")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Any-hit query round trips")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Mixed-depth query round trips")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection rays/sec")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Backend kernel rays/sec")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Closest-hit rays submitted")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Any-hit rays submitted")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Closest-hit batch average rays")).isEmpty());
  }

  TEST_F(RenderGraphInspectorWidgetTest,
         ShouldExposeDirectLightBatchAverageRowsForSelectedWavefrontPass) {
    auto trace = wavefrontDirectLightGpuTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);

    const auto rows = widget.passDetailRows(QStringLiteral("wavefront_beauty"));

    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Direct-light any-hit chunk average rays")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Direct-light any-hit round trips")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Resident direct-light round trips")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Resident direct-light savings")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Active hit host bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Direct-light selection host bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Direct-light contribution host bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Direct-light contribution execution")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Direct-light contribution fallback")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Direct-light any-hit frontier packed ray bytes")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Direct-light any-hit frontier host query bytes")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Direct-light any-hit frontier state-handle bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Closest-hit frontier residency")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Any-hit frontier residency")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Closest-hit frontier packed ray bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Any-hit frontier packed ray bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Frontier query round trips")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Resident frontier round trips")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Resident frontier savings")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Mixed query depths")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Mixed query rays")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Mixed query closest-hit rays")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Mixed query any-hit rays")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Spawned continuations")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Active host path-state bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Last active host path-state bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Last retained host path-state bytes")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Spawned continuation host path-state bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Frontier compaction passes")).isEmpty());
    EXPECT_EQ(QStringLiteral("Host"),
              rowValue(rows, QStringLiteral("Compaction path-state residency")));
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Frontier compaction input samples")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Frontier compaction retained samples")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Frontier compaction removed samples")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Frontier compaction removed fraction")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Frontier compaction moved samples")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Frontier compaction moved retained fraction")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Frontier compaction retained-index bytes")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Frontier compaction input host path-state bytes")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Frontier compaction retained host path-state bytes"))
        .isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Frontier compaction removed host path-state bytes"))
                   .isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Frontier compaction upload time")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Frontier compaction kernel time")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Frontier compaction readback time")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Supports resident frontiers")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Supports GPU frontier compaction")).isEmpty());
    const QString gpuCompactionUnavailableReason =
      rowValue(rows, QStringLiteral("GPU frontier compaction unavailable reason"));
    EXPECT_FALSE(gpuCompactionUnavailableReason.isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Supports resident direct-light batches")).isEmpty());
    const QString residentDirectLightUnavailableReason =
      rowValue(rows, QStringLiteral("Resident direct-light batches unavailable reason"));
    EXPECT_FALSE(residentDirectLightUnavailableReason.isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Compaction candidate depths")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Compaction candidate samples")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Compaction candidate host path-state bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Compaction candidate fraction")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Largest compaction candidate depth")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Largest compaction candidate samples")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Largest compaction candidate host path-state bytes"))
        .isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Largest compaction candidate fraction")).isEmpty());

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());

    QGraphicsItem* pass = graphNodeItem(graph->scene(), "pass", "wavefront_beauty");
    ASSERT_NE(nullptr, pass);

    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("mixed query depths")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("mixed-depth round trips")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("resident frontier estimate")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("resident support frontiers")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, gpuCompactionUnavailableReason));
    EXPECT_TRUE(nodeLineTooltipContains(pass, residentDirectLightUnavailableReason));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("active-hit host bytes")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("active host path-state bytes")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("final host path-state frontier")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("direct-light any-hit chunks")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("resident savings")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("contribution bytes")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("state-handle bytes")));
    EXPECT_TRUE(
      nodeLineTooltipContains(pass, QStringLiteral("host compaction on host path state removed")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("retained-index bytes")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("removed host path-state bytes")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("compaction candidate samples")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("host path-state bytes")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("largest compaction candidate")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("inactive")));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldExposeResidentPathLoopRowsForSelectedWavefrontPass) {
    auto trace = residentPathLoopTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);

    const auto rows = widget.passDetailRows(QStringLiteral("wavefront_beauty"));

    EXPECT_EQ(QStringLiteral("None"), rowValue(rows, QStringLiteral("Tracing backend platform")));
    EXPECT_EQ(QStringLiteral("Compiled CPU Reference"),
              rowValue(rows, QStringLiteral("Resident path-loop execution")));
    EXPECT_EQ(QStringLiteral("CPU Host"),
              rowValue(rows, QStringLiteral("Resident path-loop residency")));
    EXPECT_EQ(QStringLiteral("None"),
              rowValue(rows, QStringLiteral("Resident path-loop platform")));
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Resident path-loop depths")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Resident path-loop active paths by depth")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Resident path-loop input paths")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Resident path-loop retained paths")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Resident path-loop removed paths")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Resident path-loop moved paths")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Resident path-loop retained-index bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Resident path-loop path-state bytes")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Resident path-loop input path-state bytes")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Resident path-loop retained path-state bytes")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Resident path-loop removed path-state bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Resident path-loop compaction passes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Resident path-loop round trips")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Resident path-loop submitted intersection rays")).isEmpty());
    EXPECT_EQ(QStringLiteral("no"),
              rowValue(rows, QStringLiteral("Resident path-loop platform GPU kernel")));
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Resident path-loop saved host readbacks")).isEmpty());
    EXPECT_FALSE(
      rowValue(rows, QStringLiteral("Resident path-loop saved host readback bytes")).isEmpty());
    EXPECT_EQ(QStringLiteral("GPU Diffuse Path Loop"),
              rowValue(rows, QStringLiteral("Accumulation backend")));
    EXPECT_EQ(QStringLiteral("Resident Accumulation Resolve"),
              rowValue(rows, QStringLiteral("Accumulation residency")));
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Accumulation resident bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Accumulation color-sum bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Accumulation sample-count bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Accumulation moment bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Accumulation resolve bytes")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Accumulation clear operations")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Accumulation add operations")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Accumulation added samples")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Accumulation resolve operations")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Accumulation readback operations")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Accumulation readback bytes")).isEmpty());
    const QString fallbackCapabilities = rowValue(rows, QStringLiteral("Fallback capabilities"));
    EXPECT_THAT(fallbackCapabilities.toStdString(),
                ::testing::HasSubstr("Lighting Direct Light Contribution"));
    EXPECT_THAT(fallbackCapabilities.toStdString(),
                ::testing::HasSubstr("State Frontier Compaction"));
    EXPECT_THAT(fallbackCapabilities.toStdString(), ::testing::HasSubstr("GPU -> CPU"));
    EXPECT_THAT(fallbackCapabilities.toStdString(),
                ::testing::HasSubstr("platform full-GPU path-loop kernel is not available yet"));
    EXPECT_THAT(
      fallbackCapabilities.toStdString(),
      ::testing::HasSubstr("compiled CPU-reference path loop evaluates direct-light contribution"));
    const QString restrictedCapabilities =
      rowValue(rows, QStringLiteral("Restricted capabilities"));
    EXPECT_THAT(restrictedCapabilities.toStdString(), ::testing::HasSubstr("Sampling GPU RNG"));
    EXPECT_THAT(restrictedCapabilities.toStdString(),
                ::testing::HasSubstr("GPU Sample Stream CPU Reference"));
    EXPECT_THAT(restrictedCapabilities.toStdString(),
                ::testing::HasSubstr("GPU sample stream dimensions are generated"));

    const RenderPassTrace* passTrace = trace->findPass("wavefront_beauty");
    ASSERT_NE(nullptr, passTrace);
    const QJsonObject batching = passTrace->metadata().value(QStringLiteral("batching")).toObject();
    EXPECT_EQ(QStringLiteral("compiled_cpu_reference"),
              batching.value(QStringLiteral("residentPathLoopExecutionPath")).toString());
    EXPECT_EQ(QStringLiteral("none"),
              batching.value(QStringLiteral("residentPathLoopPlatformName")).toString());
    EXPECT_FALSE(batching.value(QStringLiteral("activePathsPerDepth")).toArray().isEmpty());
    EXPECT_GT(batching.value(QStringLiteral("residentPathLoopCompactionPasses")).toDouble(), 0.0);
    EXPECT_GT(
      batching.value(QStringLiteral("residentPathLoopSubmittedIntersectionRays")).toDouble(), 0.0);
    EXPECT_FALSE(batching.value(QStringLiteral("residentPathLoopFullPlatformGpuKernel")).toBool());
    EXPECT_EQ(0.0, batching.value(QStringLiteral("residentPathLoopSavedHostReadbacks")).toDouble());
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Resident path-loop saved host readbacks")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Resident path-loop saved host readback bytes")));
  }

  TEST_F(RenderGraphInspectorWidgetTest,
         ShouldExposeUnsupportedIntersectionReasonsOnWavefrontPass) {
    auto trace = unsupportedWavefrontGpuTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);

    const auto rows = widget.passDetailRows(QStringLiteral("wavefront_beauty"));

    const QString reasonRow =
      rowValue(rows, QStringLiteral("Intersection scene unsupported reasons"));
    EXPECT_TRUE(reasonRow.contains(QStringLiteral("Primitive Is Not Supported")));
    EXPECT_THAT(rowValue(rows, QStringLiteral("Tracing fallback")).toStdString(),
                ::testing::HasSubstr("GPU -> CPU"));
    EXPECT_THAT(rowValue(rows, QStringLiteral("Tracing fallback")).toStdString(),
                ::testing::HasSubstr("GPU intersection scene unsupported"));
    EXPECT_EQ(QStringLiteral("Runtime Scene"),
              rowValue(rows, QStringLiteral("Intersection backend execution path")));

    auto* graph = widget.findChild<QGraphicsView*>(QStringLiteral("renderGraphView"));
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());

    QGraphicsItem* pass = graphNodeItem(graph->scene(), "pass", "wavefront_beauty");
    ASSERT_NE(nullptr, pass);

    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("unsupported reasons")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("Primitive Is Not Supported")));
  }

  TEST_F(RenderGraphInspectorWidgetTest,
         ShouldKeepHybridVisibilityServiceTraceMetadataForSelectedPlan) {
    auto trace = hybridVisibilityTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);

    const auto rows = widget.passDetailRows(QStringLiteral("hybrid_visibility_aov"));
    EXPECT_THAT(rowValue(rows, QStringLiteral("Trace message")).toStdString(),
                ::testing::HasSubstr("intersection service"));
    EXPECT_EQ(QStringLiteral("Closest Hit"),
              rowValue(rows, QStringLiteral("Intersection service query family")));
    EXPECT_EQ(QStringLiteral("Debug AOV"),
              rowValue(rows, QStringLiteral("Intersection service tag")));
    EXPECT_EQ(QStringLiteral("CPU"),
              rowValue(rows, QStringLiteral("Intersection service backend")));
    EXPECT_EQ(QStringLiteral("Available"),
              rowValue(rows, QStringLiteral("Intersection service availability")));
    EXPECT_EQ(QStringLiteral("-"), rowValue(rows, QStringLiteral("Intersection service platform")));
    EXPECT_EQ(QStringLiteral("Runtime Scene"),
              rowValue(rows, QStringLiteral("Intersection service closest-hit path")));
    EXPECT_EQ(QStringLiteral("no"),
              rowValue(rows, QStringLiteral("Intersection service compiled scene")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Intersection service scene primitives")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Intersection service supported scene primitives")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Intersection service unsupported scene primitives")));
    EXPECT_TRUE(
      rowValue(rows, QStringLiteral("Intersection service unsupported scene reasons")).isEmpty());
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Intersection service scene upload bytes")));
    EXPECT_EQ(QStringLiteral("Host"),
              rowValue(rows, QStringLiteral("Service closest-hit frontier residency")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Service closest-hit frontier packed ray bytes")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Service closest-hit frontier host packed ray bytes")));
    EXPECT_NE(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Service closest-hit frontier host query bytes")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Service closest-hit frontier state-handle bytes")));
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection service queries")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection service hits")).isEmpty());
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Service closest-hit upload bytes")));
    EXPECT_EQ(QStringLiteral("0"), rowValue(rows, QStringLiteral("Service query transfer bytes")));

    const RenderPassTrace* passTrace = trace->findPass("hybrid_visibility_aov");
    ASSERT_NE(nullptr, passTrace);
    const QJsonObject service =
      passTrace->metadata().value(QStringLiteral("intersectionService")).toObject();
    EXPECT_EQ(QStringLiteral("closest_hit"),
              service.value(QStringLiteral("queryFamily")).toString());
    EXPECT_EQ(QStringLiteral("debug_aov"), service.value(QStringLiteral("queryTag")).toString());
    EXPECT_EQ(QStringLiteral("cpu"), service.value(QStringLiteral("requestedBackend")).toString());
    EXPECT_FALSE(service.value(QStringLiteral("compiledScene")).toBool());
    EXPECT_EQ(0.0, service.value(QStringLiteral("scenePrimitives")).toDouble());
    EXPECT_EQ(0.0, service.value(QStringLiteral("sceneSupportedPrimitives")).toDouble());
    EXPECT_EQ(0.0, service.value(QStringLiteral("sceneUnsupportedPrimitives")).toDouble());
    EXPECT_TRUE(service.value(QStringLiteral("sceneUnsupportedReasons")).toObject().isEmpty());
    EXPECT_EQ(0.0, service.value(QStringLiteral("sceneUploadBytes")).toDouble());
    EXPECT_GT(service.value(QStringLiteral("queryCount")).toDouble(), 0.0);
    EXPECT_GT(service.value(QStringLiteral("hitCount")).toDouble(), 0.0);
    EXPECT_EQ(rowValue(rows, QStringLiteral("Service closest-hit frontier host query bytes")),
              QString::number(static_cast<qulonglong>(
                service.value(QStringLiteral("closestHitFrontierHostQueryBytes")).toDouble())));
    EXPECT_EQ(rowValue(rows, QStringLiteral("Service closest-hit frontier host packed ray bytes")),
              QString::number(static_cast<qulonglong>(
                service.value(QStringLiteral("closestHitFrontierHostPackedRayBytes")).toDouble())));

    auto* graph = widget.findChild<QGraphicsView*>(QStringLiteral("renderGraphView"));
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());

    QGraphicsItem* pass = graphNodeItem(graph->scene(), "pass", "hybrid_visibility_aov");
    ASSERT_NE(nullptr, pass);

    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("intersection service Closest Hit")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("on CPU")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("via Runtime Scene")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("queries")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("hits")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("service scene runtime")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("0 primitives")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("0 supported")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("0 unsupported")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("0 upload bytes")));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowHybridServiceUnsupportedReasonsForSelectedPass) {
    auto trace = unsupportedHybridVisibilityTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);

    const auto rows = widget.passDetailRows(QStringLiteral("hybrid_visibility_aov"));
    EXPECT_EQ(QStringLiteral("yes"),
              rowValue(rows, QStringLiteral("Intersection service compiled scene")));
    EXPECT_EQ(QStringLiteral("1"),
              rowValue(rows, QStringLiteral("Intersection service scene primitives")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Intersection service supported scene primitives")));
    EXPECT_EQ(QStringLiteral("1"),
              rowValue(rows, QStringLiteral("Intersection service unsupported scene primitives")));

    const QString reasonRow =
      rowValue(rows, QStringLiteral("Intersection service unsupported scene reasons"));
    EXPECT_TRUE(reasonRow.contains(QStringLiteral("Primitive Is Not Supported")));
    EXPECT_TRUE(reasonRow.contains(QStringLiteral("1")));

    auto* graph = widget.findChild<QGraphicsView*>(QStringLiteral("renderGraphView"));
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());

    QGraphicsItem* pass = graphNodeItem(graph->scene(), "pass", "hybrid_visibility_aov");
    ASSERT_NE(nullptr, pass);

    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("service scene compiled")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("unsupported reasons")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("Primitive Is Not Supported")));
  }

  TEST_F(RenderGraphInspectorWidgetTest,
         ShouldShowHybridShadowPrimaryServiceCountsForSelectedPass) {
    auto trace = hybridRayTracedShadowTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);

    const auto rows = widget.passDetailRows(QStringLiteral("hybrid_ray_traced_shadows"));
    EXPECT_EQ(QStringLiteral("Closest Hit + Any Hit"),
              rowValue(rows, QStringLiteral("Intersection service query family")));
    EXPECT_EQ(QStringLiteral("Available"),
              rowValue(rows, QStringLiteral("Intersection service availability")));
    EXPECT_EQ(QStringLiteral("-"), rowValue(rows, QStringLiteral("Intersection service platform")));
    EXPECT_EQ(QStringLiteral("no"),
              rowValue(rows, QStringLiteral("Intersection service compiled scene")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Intersection service scene primitives")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Intersection service supported scene primitives")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Intersection service unsupported scene primitives")));
    EXPECT_TRUE(
      rowValue(rows, QStringLiteral("Intersection service unsupported scene reasons")).isEmpty());
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Intersection service scene upload bytes")));
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection service primary queries")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection service primary hits")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection service shadow queries")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Intersection service occluded queries")).isEmpty());
    EXPECT_EQ(QStringLiteral("Host"),
              rowValue(rows, QStringLiteral("Service closest-hit frontier residency")));
    EXPECT_EQ(QStringLiteral("Host"),
              rowValue(rows, QStringLiteral("Service any-hit frontier residency")));
    EXPECT_NE(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Service closest-hit frontier host query bytes")));
    EXPECT_NE(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Service any-hit frontier host query bytes")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Service closest-hit frontier packed ray bytes")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Service any-hit frontier packed ray bytes")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Service closest-hit frontier host packed ray bytes")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Service any-hit frontier host packed ray bytes")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Service closest-hit frontier state-handle bytes")));
    EXPECT_EQ(QStringLiteral("0"),
              rowValue(rows, QStringLiteral("Service any-hit frontier state-handle bytes")));

    const RenderPassTrace* passTrace = trace->findPass("hybrid_ray_traced_shadows");
    ASSERT_NE(nullptr, passTrace);
    const QJsonObject service =
      passTrace->metadata().value(QStringLiteral("intersectionService")).toObject();
    EXPECT_GT(service.value(QStringLiteral("primaryQueryCount")).toDouble(), 0.0);
    EXPECT_FALSE(service.value(QStringLiteral("compiledScene")).toBool());
    EXPECT_EQ(0.0, service.value(QStringLiteral("scenePrimitives")).toDouble());
    EXPECT_EQ(0.0, service.value(QStringLiteral("sceneSupportedPrimitives")).toDouble());
    EXPECT_EQ(0.0, service.value(QStringLiteral("sceneUnsupportedPrimitives")).toDouble());
    EXPECT_TRUE(service.value(QStringLiteral("sceneUnsupportedReasons")).toObject().isEmpty());
    EXPECT_EQ(0.0, service.value(QStringLiteral("sceneUploadBytes")).toDouble());
    EXPECT_GT(service.value(QStringLiteral("primaryHitCount")).toDouble(), 0.0);
    EXPECT_GT(service.value(QStringLiteral("shadowQueryCount")).toDouble(), 0.0);
    EXPECT_EQ(rowValue(rows, QStringLiteral("Service any-hit frontier host query bytes")),
              QString::number(static_cast<qulonglong>(
                service.value(QStringLiteral("anyHitFrontierHostQueryBytes")).toDouble())));
    EXPECT_EQ(rowValue(rows, QStringLiteral("Service any-hit frontier host packed ray bytes")),
              QString::number(static_cast<qulonglong>(
                service.value(QStringLiteral("anyHitFrontierHostPackedRayBytes")).toDouble())));

    auto* graph = widget.findChild<QGraphicsView*>(QStringLiteral("renderGraphView"));
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());

    QGraphicsItem* pass = graphNodeItem(graph->scene(), "pass", "hybrid_ray_traced_shadows");
    ASSERT_NE(nullptr, pass);

    EXPECT_TRUE(
      nodeLineTooltipContains(pass, QStringLiteral("intersection service Closest Hit + Any Hit")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("on CPU")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("via Runtime Scene")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("queries")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("hits")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("occluded")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("service scene runtime")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("0 primitives")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("0 supported")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("0 unsupported")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("0 upload bytes")));
  }

  TEST_F(RenderGraphInspectorWidgetTest,
         ShouldExposeAutoWavefrontIntersectionThresholdForSelectedPass) {
    auto trace = wavefrontAutoTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);

    const auto rows = widget.passDetailRows(QStringLiteral("wavefront_beauty"));

    EXPECT_EQ(QStringLiteral("Auto"),
              rowValue(rows, QStringLiteral("Intersection backend request")));
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Expected intersection rays")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Expected closest-hit rays")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Expected any-hit rays")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Auto minimum GPU rays")).isEmpty());
    EXPECT_FALSE(rowValue(rows, QStringLiteral("Auto estimated query transfer bytes")).isEmpty());
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowAutoWavefrontThresholdOnPassTooltip) {
    auto trace = wavefrontAutoTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());

    QGraphicsItem* pass = graphNodeItem(graph->scene(), "pass", "wavefront_beauty");
    ASSERT_NE(nullptr, pass);

    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("auto GPU threshold")));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowWavefrontQueryRoundTripFamiliesOnPassTooltip) {
    auto trace = wavefrontDirectLightGpuTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());

    QGraphicsItem* pass = graphNodeItem(graph->scene(), "pass", "wavefront_beauty");
    ASSERT_NE(nullptr, pass);

    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("query round trips")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("closest-hit")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("any-hit")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("mixed-depth readback bytes")));
    EXPECT_TRUE(nodeLineTooltipContains(pass, QStringLiteral("active-hit host bytes")));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowRasterMetricsOnSelectedPassRow) {
    auto trace = rasterTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);

    auto* passes = widget.findChild<QTreeWidget*>("renderGraphPasses");
    ASSERT_NE(nullptr, passes);
    ASSERT_GT(passes->topLevelItemCount(), 0);

    QTreeWidgetItem* raster = nullptr;
    for (int row = 0; row != passes->topLevelItemCount(); ++row) {
      if (passes->topLevelItem(row)->data(0, Qt::UserRole).toString() ==
          QStringLiteral("raster_beauty")) {
        raster = passes->topLevelItem(row);
        break;
      }
    }

    ASSERT_NE(nullptr, raster);
    EXPECT_TRUE(raster->text(4).contains(QStringLiteral("completed")));
    EXPECT_TRUE(raster->text(4).contains(QStringLiteral("shaded")));
    EXPECT_TRUE(raster->text(4).contains(QStringLiteral("writes")));
    EXPECT_TRUE(raster->text(4).contains(QStringLiteral("queue")));
    EXPECT_TRUE(raster->text(4).contains(QStringLiteral("prepass")));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldEmitPassSelectedFromGraphNode) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(twoPassPlan());
    QString selectedPass;
    QObject::connect(&widget, &RenderGraphInspectorWidget::passSelected,
                     [&](const QString& passId) { selectedPass = passId; });

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());
    QGraphicsItem* tonemap = graphItem(graph->scene(), "pass", "tonemap");
    ASSERT_NE(nullptr, tonemap);

    QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMousePress);
    event.setScenePos(tonemap->sceneBoundingRect().center());
    QApplication::sendEvent(graph->scene(), &event);

    EXPECT_EQ(QString("tonemap"), selectedPass);
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldEmitResourceSelectedFromGraphNode) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(twoPassPlan());
    QString selectedResource;
    QObject::connect(&widget, &RenderGraphInspectorWidget::resourceSelected,
                     [&](const QString& resourceId) { selectedResource = resourceId; });

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());
    QGraphicsItem* resource = graphItem(graph->scene(), "resource", "beauty_color");
    ASSERT_NE(nullptr, resource);

    QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMousePress);
    event.setScenePos(resource->sceneBoundingRect().center());
    QApplication::sendEvent(graph->scene(), &event);

    EXPECT_EQ(QString("beauty_color"), selectedResource);
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldEmitSelectionTraceChangedWhenTraceChanges) {
    auto trace = postProcessTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    selectPass(widget, QStringLiteral("post_fxaa"));

    QString selectedPass;
    QObject::connect(&widget, &RenderGraphInspectorWidget::selectedPassTraceChanged,
                     [&](const QString& passId) { selectedPass = passId; });

    widget.setExecutionTrace(trace);

    EXPECT_EQ(QString("post_fxaa"), selectedPass);
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldEmitResourceTraceChangedWhenTraceChanges) {
    auto trace = postProcessTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    selectResource(widget, QStringLiteral("post_aa_color"));

    QString selectedResource;
    QObject::connect(&widget, &RenderGraphInspectorWidget::selectedResourceTraceChanged,
                     [&](const QString& resourceId) { selectedResource = resourceId; });

    widget.setExecutionTrace(trace);

    EXPECT_EQ(QString("post_aa_color"), selectedResource);
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldDisablePassThroughCheckboxOverride) {
    RenderGraphInspectorWidget widget;
    Slot slot;
    QObject::connect(&widget, SIGNAL(overridesChanged()), &slot, SLOT(receive()));
    widget.setPlan(simplePlan());

    auto* passes = widget.findChild<QTreeWidget*>("renderGraphPasses");
    ASSERT_NE(nullptr, passes);
    ASSERT_EQ(1, passes->topLevelItemCount());

    passes->topLevelItem(0)->setCheckState(0, Qt::Unchecked);

    const auto overrides = widget.overrides();
    EXPECT_TRUE(slot.called());
    EXPECT_NE(overrides.disabledPasses.end(), overrides.disabledPasses.find("raytrace_beauty"));
    EXPECT_FALSE(widget.effectivePlanValid());
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldKeepGraphNodeLabelsInsideNodeBounds) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(longLabelPlan());

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());

    auto* passNode = graphNodeItem(graph->scene(), "pass", "raytrace_beauty");
    ASSERT_NE(nullptr, passNode);

    const QRectF nodeRect = passNode->boundingRect();
    for (QGraphicsItem* child : passNode->childItems()) {
      auto* label = dynamic_cast<QGraphicsSimpleTextItem*>(child);
      if (!label)
        continue;
      EXPECT_LE(label->pos().x() + label->boundingRect().width(), nodeRect.right());
    }
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldDropOverridesForMissingPassesOnNewPlan) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(simplePlan());

    auto* passes = widget.findChild<QTreeWidget*>("renderGraphPasses");
    ASSERT_NE(nullptr, passes);
    passes->topLevelItem(0)->setCheckState(0, Qt::Unchecked);

    RenderPlan replacement;
    widget.setPlan(replacement);

    EXPECT_TRUE(widget.overrides().disabledPasses.empty());
  }
}
