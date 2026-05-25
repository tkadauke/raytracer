#include <gtest/gtest.h>

#include "widgets/world/RenderGraphTracePreviewWidget.h"

#include "core/Buffer.h"
#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RenderGraphCompiler.h"
#include "engine/graph/RenderGraphExecutionTrace.h"
#include "engine/graph/RenderResourceStorage.h"
#include "render/cameras/PinholeCamera.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"
#include "test/helpers/GuiTestHelper.h"

#include <QLabel>
#include <QTabWidget>
#include <QWidget>

namespace RenderGraphTracePreviewWidgetTest {
  using namespace engine::graph;

  class RenderGraphTracePreviewWidgetTest : public ::testing::GuiTest {};

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

  std::shared_ptr<const RenderGraphExecutionTrace> shadowMapTrace() {
    RenderPlan plan;
    RenderResourceDescriptor shadowMap;
    shadowMap.id = "preview_shadow_map";
    shadowMap.type = RenderResourceType::ShadowMap;
    shadowMap.format = RenderResourceFormat::DepthDouble;
    shadowMap.width = 8;
    shadowMap.height = 8;
    plan.addResource(shadowMap);

    RenderPassNode shadowPass;
    shadowPass.id = "raster_preview_shadows";
    shadowPass.kind = RenderPassKind::Shadow;
    shadowPass.executor = RenderExecutorKind::Rasterizer;
    shadowPass.writes.push_back({"preview_shadow_map"});
    plan.addPass(shadowPass);

    RenderResourceStorage storage;
    storage.allocate(plan.resources());
    storage.depth("preview_shadow_map").clear(4.0);
    storage.depth("preview_shadow_map")[3][4] = 1.0;

    RenderGraphExecutionTraceRecorder recorder;
    const auto session = recorder.begin(plan);
    recorder.passStarted(session, plan.passes().front(), storage);
    recorder.passCompleted(session, plan.passes().front(), storage);
    recorder.finish(session);
    return recorder.lastTrace();
  }

  std::shared_ptr<const RenderGraphExecutionTrace> metadataOnlyShadowTrace() {
    RenderPlan plan;
    RenderResourceDescriptor shadowMap;
    shadowMap.id = "preview_shadow_map";
    shadowMap.type = RenderResourceType::ShadowMap;
    shadowMap.format = RenderResourceFormat::DepthDouble;
    shadowMap.width = 8;
    shadowMap.height = 8;
    shadowMap.domain = RenderResourceDomain::GPU;
    plan.addResource(shadowMap);

    RenderPassNode shadowPass;
    shadowPass.id = "raster_preview_shadows";
    shadowPass.kind = RenderPassKind::Shadow;
    shadowPass.executor = RenderExecutorKind::Rasterizer;
    shadowPass.writes.push_back({"preview_shadow_map"});
    plan.addPass(shadowPass);

    RenderResourceStorage storage;
    storage.allocate(plan.resources());

    RenderGraphExecutionTraceRecorder recorder;
    const auto session = recorder.begin(plan);
    recorder.passStarted(session, plan.passes().front(), storage);
    recorder.passCompleted(session, plan.passes().front(), storage);
    recorder.finish(session);
    return recorder.lastTrace();
  }

  bool labelsContain(QWidget* root, const QString& text) {
    for (QLabel* label : root->findChildren<QLabel*>()) {
      if (label->text().contains(text)) {
        return true;
      }
    }
    return false;
  }

  TEST_F(RenderGraphTracePreviewWidgetTest, ShouldInitialize) {
    RenderGraphTracePreviewWidget widget;

    auto* tabs = widget.findChild<QTabWidget*>("renderGraphTracePreviewTabs");
    ASSERT_NE(nullptr, tabs);
    EXPECT_EQ(3, tabs->count());
  }

  TEST_F(RenderGraphTracePreviewWidgetTest, ShouldShowPassTracePreviews) {
    auto trace = postProcessTrace();
    ASSERT_TRUE(trace);

    RenderGraphTracePreviewWidget widget;
    widget.showPassTrace(trace, "post_fxaa");

    auto* title = widget.findChild<QLabel*>("renderGraphTracePreviewTitle");
    auto* inputs = widget.findChild<QWidget*>("renderGraphTracePreviewInputs");
    auto* outputs = widget.findChild<QWidget*>("renderGraphTracePreviewOutputs");
    auto* diffs = widget.findChild<QWidget*>("renderGraphTracePreviewDifferences");
    ASSERT_NE(nullptr, title);
    ASSERT_NE(nullptr, inputs);
    ASSERT_NE(nullptr, outputs);
    ASSERT_NE(nullptr, diffs);

    EXPECT_TRUE(title->text().contains(QStringLiteral("post_fxaa")));
    EXPECT_TRUE(labelsContain(inputs, QStringLiteral("beauty_color")));
    EXPECT_TRUE(labelsContain(outputs, QStringLiteral("post_aa_color")));
    EXPECT_TRUE(labelsContain(diffs, QStringLiteral("Boosted difference")));
  }

  TEST_F(RenderGraphTracePreviewWidgetTest, ShouldScaleTraceImagesForInspection) {
    auto trace = postProcessTrace();
    ASSERT_TRUE(trace);

    RenderGraphTracePreviewWidget widget;
    widget.showPassTrace(trace, "post_fxaa");

    auto images = widget.findChildren<QLabel*>("renderGraphTracePreviewImage");
    ASSERT_FALSE(images.empty());
    EXPECT_GE(images.front()->pixmap().width(), 640);
  }

  TEST_F(RenderGraphTracePreviewWidgetTest, ShouldShowResourceTracePreviews) {
    auto trace = postProcessTrace();
    ASSERT_TRUE(trace);
    const auto outputSnapshots = trace->outputSnapshotsForResource("post_aa_color");
    ASSERT_EQ(1u, outputSnapshots.size());
    EXPECT_TRUE(outputSnapshots.front()->hasPreview());
    EXPECT_TRUE(trace->hasResourceSnapshots("post_aa_color"));
    EXPECT_FALSE(trace->hasResourceSnapshots("missing_resource"));

    RenderGraphTracePreviewWidget widget;
    widget.showResourceTrace(trace, "post_aa_color");

    auto* title = widget.findChild<QLabel*>("renderGraphTracePreviewTitle");
    auto* outputs = widget.findChild<QWidget*>("renderGraphTracePreviewOutputs");
    ASSERT_NE(nullptr, title);
    ASSERT_NE(nullptr, outputs);

    EXPECT_TRUE(title->text().contains(QStringLiteral("post_aa_color")));
    EXPECT_TRUE(labelsContain(outputs, QStringLiteral("post_aa_color")));
  }

  TEST_F(RenderGraphTracePreviewWidgetTest, ShouldExplainDeclaredResourceWithoutReaders) {
    auto trace = postProcessTrace();
    ASSERT_TRUE(trace);

    RenderGraphTracePreviewWidget widget;
    widget.showResourceTrace(trace, "main_color");

    auto* inputs = widget.findChild<QWidget*>("renderGraphTracePreviewInputs");
    ASSERT_NE(nullptr, inputs);

    EXPECT_TRUE(
      labelsContain(inputs, QStringLiteral("No pass read this resource during this execution")));
  }

  TEST_F(RenderGraphTracePreviewWidgetTest, ShouldExplainMissingResourceTrace) {
    auto trace = postProcessTrace();
    ASSERT_TRUE(trace);

    RenderGraphTracePreviewWidget widget;
    widget.showResourceTrace(trace, "missing_resource");

    auto* outputs = widget.findChild<QWidget*>("renderGraphTracePreviewOutputs");
    ASSERT_NE(nullptr, outputs);

    EXPECT_TRUE(
      labelsContain(outputs, QStringLiteral("Resource is not declared by this execution trace")));
  }

  TEST_F(RenderGraphTracePreviewWidgetTest, ShouldShowDepthResourceTrace) {
    auto trace = shadowMapTrace();
    ASSERT_TRUE(trace);

    RenderGraphTracePreviewWidget widget;
    widget.showResourceTrace(trace, "preview_shadow_map");

    auto* outputs = widget.findChild<QWidget*>("renderGraphTracePreviewOutputs");
    ASSERT_NE(nullptr, outputs);

    EXPECT_TRUE(labelsContain(outputs, QStringLiteral("preview_shadow_map")));
    EXPECT_FALSE(labelsContain(outputs, QStringLiteral("preview is not available")));
    EXPECT_FALSE(widget.findChildren<QLabel*>("renderGraphTracePreviewImage").empty());
  }

  TEST_F(RenderGraphTracePreviewWidgetTest, ShouldShowMetadataOnlyResourceTrace) {
    auto trace = metadataOnlyShadowTrace();
    ASSERT_TRUE(trace);
    const auto outputSnapshots = trace->outputSnapshotsForResource("preview_shadow_map");
    ASSERT_EQ(1u, outputSnapshots.size());
    EXPECT_FALSE(outputSnapshots.front()->hasPreview());

    RenderGraphTracePreviewWidget widget;
    widget.showResourceTrace(trace, "preview_shadow_map");

    auto* outputs = widget.findChild<QWidget*>("renderGraphTracePreviewOutputs");
    ASSERT_NE(nullptr, outputs);

    EXPECT_TRUE(labelsContain(outputs, QStringLiteral("preview_shadow_map")));
    EXPECT_TRUE(labelsContain(outputs, QStringLiteral("preview is not available")));
    EXPECT_TRUE(widget.findChildren<QLabel*>("renderGraphTracePreviewImage").empty());
  }
}
