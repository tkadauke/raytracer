#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "widgets/world/RenderGraphInspectorWidget.h"

#include "core/Buffer.h"
#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RenderGraphCompiler.h"
#include "engine/graph/RenderGraphExecutionTrace.h"
#include "render/cameras/PinholeCamera.h"
#include "render/materials/MatteMaterial.h"
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
    pass.disabledBehavior = DisabledBehavior::Error;
    plan.addPass(pass);

    return plan;
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
      if (item->text(3) == passId) {
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
      if (item->text(0) == resourceId) {
        resources->setCurrentItem(item);
        return;
      }
    }
    FAIL() << "missing resource row " << resourceId.toStdString();
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
    EXPECT_EQ(QString("raytrace_beauty"), passes->topLevelItem(0)->text(3));
    EXPECT_EQ(QString("beauty"), passes->topLevelItem(0)->text(4));
    EXPECT_EQ(QString("raytracer"), passes->topLevelItem(0)->text(5));
    EXPECT_EQ(QString("all"), passes->topLevelItem(0)->text(6));
    EXPECT_EQ(QString("preview-camera"), passes->topLevelItem(0)->text(7));
    EXPECT_EQ(QString("main_color"), passes->topLevelItem(0)->text(9));

    ASSERT_NE(nullptr, graph->scene());
    auto* passNode = graphNodeItem(graph->scene(), "pass", "raytrace_beauty");
    ASSERT_NE(nullptr, passNode);
    EXPECT_TRUE(nodeTextContains(passNode, "stage 1"));
    EXPECT_TRUE(passNode->toolTip().contains("Reads: -"));
    EXPECT_TRUE(passNode->toolTip().contains("Writes: main_color"));
    EXPECT_TRUE(passNode->toolTip().contains("Incoming dependencies: -"));
    auto* resourceNode = graphNodeItem(graph->scene(), "resource", "main_color");
    ASSERT_NE(nullptr, resourceNode);
    EXPECT_TRUE(nodeTextContains(resourceNode, "rgb_double"));
    EXPECT_TRUE(nodeTextContains(resourceNode, "exported"));
    EXPECT_TRUE(resourceNode->toolTip().contains("Producer: raytrace_beauty"));
    EXPECT_TRUE(resourceNode->toolTip().contains("Consumers: -"));

    ASSERT_EQ(1, resources->topLevelItemCount());
    EXPECT_EQ(QString("main_color"), resources->topLevelItem(0)->text(0));
    EXPECT_EQ(QString("raytrace_beauty"), resources->topLevelItem(0)->text(1));
    EXPECT_EQ(QString("-"), resources->topLevelItem(0)->text(2));
    EXPECT_EQ(QString("color"), resources->topLevelItem(0)->text(3));
    EXPECT_THAT(status->text().toStdString(), ::testing::HasSubstr("Valid plan"));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldDisablePassesByGroupOverride) {
    RenderGraphInspectorWidget widget;
    Slot slot;
    QObject::connect(&widget, SIGNAL(overridesChanged()), &slot, SLOT(receive()));
    widget.setPlan(twoPassPlan());

    QTreeWidgetItem* tonemapKind =
      groupItem(widget, QStringLiteral("Kind"), QStringLiteral("tonemap"));
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

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowResourceEdgeRows) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(twoPassPlan());

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    EXPECT_EQ(nullptr, widget.findChild<QTreeWidget*>("renderGraphDependencies"));

    auto* resources = widget.findChild<QTreeWidget*>("renderGraphResources");
    ASSERT_NE(nullptr, resources);
    ASSERT_EQ(2, resources->topLevelItemCount());
    EXPECT_EQ(QString("beauty_color"), resources->topLevelItem(0)->text(0));
    EXPECT_EQ(QString("raytrace_beauty"), resources->topLevelItem(0)->text(1));
    EXPECT_EQ(QString("tonemap"), resources->topLevelItem(0)->text(2));
    EXPECT_NE(nullptr, graphItem(graph->scene(), "pass", "raytrace_beauty"));
    EXPECT_NE(nullptr, graphItem(graph->scene(), "pass", "tonemap"));
    EXPECT_NE(nullptr, graphItem(graph->scene(), "resource", "beauty_color"));
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
