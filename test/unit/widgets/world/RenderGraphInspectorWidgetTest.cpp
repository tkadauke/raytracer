#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "widgets/world/RenderGraphInspectorWidget.h"

#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/Slot.h"

#include <QLabel>
#include <QTreeWidget>

namespace RenderGraphInspectorWidgetTest {
  using namespace engine::graph;

  class RenderGraphInspectorWidgetTest : public ::testing::GuiTest {};

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

  TEST_F(RenderGraphInspectorWidgetTest, ShouldInitialize) {
    RenderGraphInspectorWidget widget;
  }

  TEST_F(RenderGraphInspectorWidgetTest, ShouldShowPassAndResourceRows) {
    RenderGraphInspectorWidget widget;
    widget.setPlan(simplePlan());

    auto* passes = widget.findChild<QTreeWidget*>("renderGraphPasses");
    auto* resources = widget.findChild<QTreeWidget*>("renderGraphResources");
    auto* status = widget.findChild<QLabel*>("renderGraphValidationStatus");
    ASSERT_NE(nullptr, passes);
    ASSERT_NE(nullptr, resources);
    ASSERT_NE(nullptr, status);

    ASSERT_EQ(1, passes->topLevelItemCount());
    EXPECT_EQ(QString("raytrace_beauty"), passes->topLevelItem(0)->text(1));
    EXPECT_EQ(QString("beauty"), passes->topLevelItem(0)->text(2));
    EXPECT_EQ(QString("raytracer"), passes->topLevelItem(0)->text(3));
    EXPECT_EQ(QString("main_color"), passes->topLevelItem(0)->text(5));

    ASSERT_EQ(1, resources->topLevelItemCount());
    EXPECT_EQ(QString("main_color"), resources->topLevelItem(0)->text(0));
    EXPECT_EQ(QString("color"), resources->topLevelItem(0)->text(1));
    EXPECT_THAT(status->text().toStdString(), ::testing::HasSubstr("Valid plan"));
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
