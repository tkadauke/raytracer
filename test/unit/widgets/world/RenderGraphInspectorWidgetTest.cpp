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
#include <QGraphicsView>
#include <QLabel>
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
    engine.setPlan(compiler.compile({24, 24, 1}, intent));

    Buffer<unsigned int> buffer(24, 24);
    engine.render(buffer);
    return engine.lastExecutionTrace();
  }

  bool labelsContain(QWidget* root, const QString& text) {
    for (QLabel* label : root->findChildren<QLabel*>()) {
      if (label->text().contains(text)) {
        return true;
      }
    }
    return false;
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
      if (item->text(2) == passId) {
        passes->setCurrentItem(item);
        return;
      }
    }
    FAIL() << "missing pass row " << passId.toStdString();
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
    EXPECT_EQ(QString("raytrace_beauty"), passes->topLevelItem(0)->text(2));
    EXPECT_EQ(QString("beauty"), passes->topLevelItem(0)->text(3));
    EXPECT_EQ(QString("raytracer"), passes->topLevelItem(0)->text(4));
    EXPECT_EQ(QString("main_color"), passes->topLevelItem(0)->text(6));

    ASSERT_NE(nullptr, graph->scene());
    EXPECT_NE(nullptr, graphItem(graph->scene(), "pass", "raytrace_beauty"));
    EXPECT_NE(nullptr, graphItem(graph->scene(), "resource", "main_color"));

    ASSERT_EQ(1, resources->topLevelItemCount());
    EXPECT_EQ(QString("main_color"), resources->topLevelItem(0)->text(0));
    EXPECT_EQ(QString("raytrace_beauty"), resources->topLevelItem(0)->text(1));
    EXPECT_EQ(QString("-"), resources->topLevelItem(0)->text(2));
    EXPECT_EQ(QString("color"), resources->topLevelItem(0)->text(3));
    EXPECT_THAT(status->text().toStdString(), ::testing::HasSubstr("Valid plan"));
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

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowExecutionTraceForSelectedPass) {
    auto trace = postProcessTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);
    selectPass(widget, QStringLiteral("post_fxaa"));

    auto* title = widget.findChild<QLabel*>("renderGraphTraceTitle");
    auto* inputs = widget.findChild<QWidget*>("renderGraphTraceInputs");
    auto* outputs = widget.findChild<QWidget*>("renderGraphTraceOutputs");
    auto* diffs = widget.findChild<QWidget*>("renderGraphTraceDifferences");
    auto* metadata = widget.findChild<QTreeWidget*>("renderGraphTraceMetadata");
    ASSERT_NE(nullptr, title);
    ASSERT_NE(nullptr, inputs);
    ASSERT_NE(nullptr, outputs);
    ASSERT_NE(nullptr, diffs);
    ASSERT_NE(nullptr, metadata);

    EXPECT_THAT(title->text().toStdString(), ::testing::HasSubstr("post_fxaa"));
    EXPECT_TRUE(labelsContain(inputs, QStringLiteral("beauty_color")));
    EXPECT_TRUE(labelsContain(outputs, QStringLiteral("post_aa_color")));
    EXPECT_TRUE(labelsContain(diffs, QStringLiteral("Boosted difference")));

    ASSERT_GE(metadata->topLevelItemCount(), 2);
    EXPECT_EQ(QString("Pass"), metadata->topLevelItem(0)->text(0));
    EXPECT_EQ(QString("post_fxaa"), metadata->topLevelItem(0)->text(1));
    EXPECT_EQ(QString("Status"), metadata->topLevelItem(1)->text(0));
    EXPECT_EQ(QString("completed"), metadata->topLevelItem(1)->text(1));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldSelectTracePassFromGraphNode) {
    auto trace = postProcessTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->scene());
    QGraphicsItem* postAA = graphItem(graph->scene(), "pass", "post_fxaa");
    ASSERT_NE(nullptr, postAA);

    QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMousePress);
    event.setScenePos(postAA->sceneBoundingRect().center());
    QApplication::sendEvent(graph->scene(), &event);

    auto* title = widget.findChild<QLabel*>("renderGraphTraceTitle");
    ASSERT_NE(nullptr, title);
    EXPECT_THAT(title->text().toStdString(), ::testing::HasSubstr("post_fxaa"));
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

  TEST_F(RenderGraphInspectorWidgetTest, ShouldClearStaleExecutionTraceOnRenderStart) {
    auto trace = postProcessTrace();
    ASSERT_TRUE(trace);

    RenderGraphInspectorWidget widget;
    widget.setPlan(trace->plan());
    widget.setExecutionTrace(trace);
    selectPass(widget, QStringLiteral("post_fxaa"));

    widget.clearExecutionState();

    auto* title = widget.findChild<QLabel*>("renderGraphTraceTitle");
    auto* inputs = widget.findChild<QWidget*>("renderGraphTraceInputs");
    ASSERT_NE(nullptr, title);
    ASSERT_NE(nullptr, inputs);
    EXPECT_THAT(title->text().toStdString(), ::testing::HasSubstr("post_fxaa"));
    EXPECT_TRUE(labelsContain(inputs, QStringLiteral("No execution trace")));
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
