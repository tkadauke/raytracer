#include <gtest/gtest.h>

#include "engine/graph/RenderGraphRequest.h"

namespace RenderGraphRequestTest {
  using namespace engine::graph;

  TEST(RenderGraphRequest, ResolvesExplicitOverridesOnTopOfSceneIntent) {
    RenderIntent base;
    base.defaultExecutor = RenderExecutorPreference::Raytracer;
    base.defaultViewMode = RenderViewMode::Beauty;
    base.postProcessAA = RenderPostProcessAA::SMAA;
    base.enablePreviewShadows = true;
    base.enableWireframeOverlay = true;

    RenderGraphRequest request(base);
    request.setExecutorOverride(RenderExecutorPreference::Rasterizer)
      .setViewModeOverride(RenderViewMode::Depth)
      .setPostProcessAAOverride(RenderPostProcessAA::None)
      .setPreviewShadowsOverride(false)
      .setWireframeOverlayOverride(false)
      .setCurveOverlayOverride(true)
      .requestExportedAOV(RenderViewMode::Stencil);

    const RenderIntent resolved = request.resolvedIntent();

    EXPECT_EQ(RenderExecutorPreference::Rasterizer, resolved.defaultExecutor);
    EXPECT_EQ(RenderViewMode::Depth, resolved.defaultViewMode);
    EXPECT_EQ(RenderPostProcessAA::None, resolved.postProcessAA);
    EXPECT_FALSE(resolved.enablePreviewShadows);
    EXPECT_FALSE(resolved.enableWireframeOverlay);
    EXPECT_TRUE(resolved.enableCurveOverlay);
    ASSERT_EQ(1u, resolved.exportedAOVs.size());
    EXPECT_EQ(RenderViewMode::Stencil, resolved.exportedAOVs.front());
  }

  TEST(RenderGraphRequest, WireframeExecutorShortcutSelectsWireframeView) {
    RenderGraphRequest request;
    request.setExecutorShortcut(RenderExecutorPreference::Wireframe);

    const RenderIntent resolved = request.resolvedIntent();

    EXPECT_EQ(RenderExecutorPreference::Wireframe, resolved.defaultExecutor);
    EXPECT_EQ(RenderViewMode::Wireframe, resolved.defaultViewMode);
  }

  TEST(RenderGraphRequest, ExplicitViewBeatsWireframeExecutorShortcut) {
    RenderGraphRequest request;
    request.setExecutorShortcut(RenderExecutorPreference::Wireframe)
      .setViewModeOverride(RenderViewMode::Stencil);

    const RenderIntent resolved = request.resolvedIntent();

    EXPECT_EQ(RenderExecutorPreference::Wireframe, resolved.defaultExecutor);
    EXPECT_EQ(RenderViewMode::Stencil, resolved.defaultViewMode);
  }

  TEST(RenderGraphRequest, ExplicitExecutorDoesNotImplyWireframeView) {
    RenderGraphRequest request;
    request.setExecutorOverride(RenderExecutorPreference::Wireframe);

    const RenderIntent resolved = request.resolvedIntent();

    EXPECT_EQ(RenderExecutorPreference::Wireframe, resolved.defaultExecutor);
    EXPECT_EQ(RenderViewMode::Beauty, resolved.defaultViewMode);
  }

  TEST(RenderGraphRequest, CompileUsesResolvedIntent) {
    RenderGraphRequest request;
    request.setExecutorOverride(RenderExecutorPreference::Rasterizer)
      .setViewModeOverride(RenderViewMode::Stencil);

    const RenderPlan plan = request.compile({16, 8, 1});

    ASSERT_NE(nullptr, plan.findPass("stencil_aov"));
    EXPECT_EQ(RenderExecutorKind::Rasterizer, plan.findPass("stencil_aov")->executor);
  }
}
