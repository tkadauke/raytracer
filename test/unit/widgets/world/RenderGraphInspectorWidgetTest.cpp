#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "widgets/world/RenderGraphInspectorWidget.h"

#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/Slot.h"

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QLabel>
#include <QTreeWidget>

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

  TEST_F(RenderGraphInspectorWidgetTest, ShouldInitialize) {
    RenderGraphInspectorWidget widget;
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowPassAndResourceRows) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(simplePlan());

    auto* passes = widget.findChild<QTreeWidget*>("renderGraphPasses");
    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    auto* dependencies = widget.findChild<QTreeWidget*>("renderGraphDependencies");
    auto* resources = widget.findChild<QTreeWidget*>("renderGraphResources");
    auto* status = widget.findChild<QLabel*>("renderGraphValidationStatus");
    ASSERT_NE(nullptr, passes);
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, dependencies);
    ASSERT_NE(nullptr, resources);
    ASSERT_NE(nullptr, status);

    ASSERT_EQ(1, passes->topLevelItemCount());
    EXPECT_EQ(QString("1"), passes->topLevelItem(0)->text(1));
    EXPECT_EQ(QString("raytrace_beauty"), passes->topLevelItem(0)->text(2));
    EXPECT_EQ(QString("beauty"), passes->topLevelItem(0)->text(3));
    EXPECT_EQ(QString("raytracer"), passes->topLevelItem(0)->text(4));
    EXPECT_EQ(QString("main_color"), passes->topLevelItem(0)->text(6));

    ASSERT_NE(nullptr, graph->scene());
    EXPECT_NE(nullptr, graphItem(graph->scene(), "pass", "raytrace_beauty"));
    EXPECT_NE(nullptr, graphItem(graph->scene(), "resource", "main_color"));

    EXPECT_EQ(0, dependencies->topLevelItemCount());

    ASSERT_EQ(1, resources->topLevelItemCount());
    EXPECT_EQ(QString("main_color"), resources->topLevelItem(0)->text(0));
    EXPECT_EQ(QString("raytrace_beauty"), resources->topLevelItem(0)->text(1));
    EXPECT_EQ(QString("-"), resources->topLevelItem(0)->text(2));
    EXPECT_EQ(QString("color"), resources->topLevelItem(0)->text(3));
    EXPECT_THAT(status->text().toStdString(), ::testing::HasSubstr("Valid plan"));
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowDependencyRows) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(twoPassPlan());

    auto* dependencies = widget.findChild<QTreeWidget*>("renderGraphDependencies");
    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, dependencies);
    ASSERT_NE(nullptr, graph);
    ASSERT_EQ(1, dependencies->topLevelItemCount());
    EXPECT_EQ(QString("raytrace_beauty"), dependencies->topLevelItem(0)->text(0));
    EXPECT_EQ(QString("beauty_color"), dependencies->topLevelItem(0)->text(1));
    EXPECT_EQ(QString("tonemap"), dependencies->topLevelItem(0)->text(2));

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

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowLiveExecutionStateOnGraphNodes) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(twoPassPlan());

    auto* graph = widget.findChild<QGraphicsView*>("renderGraphView");
    ASSERT_NE(nullptr, graph);

    widget.passExecutionStarted(QStringLiteral("raytrace_beauty"));
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
