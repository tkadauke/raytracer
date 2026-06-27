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
      .setRasterBackendOverride(engine::raster::RasterBackend::openGL())
      .setWireframeOverlayOverride(false)
      .setCurveOverlayOverride(true)
      .requestExportedAOV(RenderViewMode::Stencil);

    const RenderIntent resolved = request.resolvedIntent();

    EXPECT_EQ(RenderExecutorPreference::Rasterizer, resolved.defaultExecutor);
    EXPECT_EQ(RenderViewMode::Depth, resolved.defaultViewMode);
    EXPECT_EQ(RenderPostProcessAA::None, resolved.postProcessAA);
    EXPECT_FALSE(resolved.enablePreviewShadows);
    ASSERT_TRUE(resolved.engineOptions.rasterizer().backend().has_value());
    EXPECT_TRUE(resolved.engineOptions.rasterizer().backend()->isOpenGL());
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

  TEST(RenderGraphRequest, PathTracerShortcutDefaultsToGpuPathLoopIntent) {
    RenderGraphRequest request;
    request.setExecutorShortcut(RenderExecutorPreference::PathTracer);

    const RenderIntent resolved = request.resolvedIntent();
    const auto& raytracer = resolved.engineOptions.raytracer();

    EXPECT_EQ(RenderExecutorPreference::PathTracer, resolved.defaultExecutor);
    ASSERT_TRUE(raytracer.integrator().has_value());
    EXPECT_EQ("pathtracer", *raytracer.integrator());
    ASSERT_TRUE(raytracer.sampleStreamMode().has_value());
    EXPECT_EQ("gpu_sample_stream", *raytracer.sampleStreamMode());
    ASSERT_TRUE(raytracer.tracingExecution().has_value());
    EXPECT_EQ(TracingExecutionPreference::GPU, *raytracer.tracingExecution());
  }

  TEST(RenderGraphRequest, PathTracerSceneIntentDefaultsToGpuPathLoopIntent) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    RenderGraphRequest request(intent);

    const RenderIntent resolved = request.resolvedIntent();
    const auto& raytracer = resolved.engineOptions.raytracer();

    ASSERT_TRUE(raytracer.sampleStreamMode().has_value());
    EXPECT_EQ("gpu_sample_stream", *raytracer.sampleStreamMode());
    ASSERT_TRUE(raytracer.tracingExecution().has_value());
    EXPECT_EQ(TracingExecutionPreference::GPU, *raytracer.tracingExecution());
  }

  TEST(RenderGraphRequest, PathTracerShortcutPreservesExplicitSamplerAsHostDriven) {
    RenderIntent intent;
    intent.engineOptions.raytracer().setSampler("Halton");
    RenderGraphRequest request(intent);
    request.setExecutorShortcut(RenderExecutorPreference::PathTracer);

    const RenderIntent resolved = request.resolvedIntent();
    const auto& raytracer = resolved.engineOptions.raytracer();

    ASSERT_TRUE(raytracer.integrator().has_value());
    EXPECT_EQ("pathtracer", *raytracer.integrator());
    ASSERT_TRUE(raytracer.sampler().has_value());
    EXPECT_EQ("Halton", *raytracer.sampler());
    EXPECT_FALSE(raytracer.sampleStreamMode().has_value());
    EXPECT_FALSE(raytracer.tracingExecution().has_value());
  }

  TEST(RenderGraphRequest, PathTracerShortcutPreservesExplicitCpuTracingExecution) {
    RenderIntent intent;
    intent.engineOptions.raytracer().setTracingExecution(TracingExecutionPreference::CPU);
    RenderGraphRequest request(intent);
    request.setExecutorShortcut(RenderExecutorPreference::PathTracer);

    const RenderIntent resolved = request.resolvedIntent();
    const auto& raytracer = resolved.engineOptions.raytracer();

    ASSERT_TRUE(raytracer.tracingExecution().has_value());
    EXPECT_EQ(TracingExecutionPreference::CPU, *raytracer.tracingExecution());
    EXPECT_FALSE(raytracer.sampleStreamMode().has_value());
  }

  TEST(RenderGraphRequest, PathTracerShortcutPreservesExplicitAutoWhileUsingGpuStream) {
    RenderIntent intent;
    intent.engineOptions.raytracer().setTracingExecution(TracingExecutionPreference::Auto);
    RenderGraphRequest request(intent);
    request.setExecutorShortcut(RenderExecutorPreference::PathTracer);

    const RenderIntent resolved = request.resolvedIntent();
    const auto& raytracer = resolved.engineOptions.raytracer();

    ASSERT_TRUE(raytracer.tracingExecution().has_value());
    EXPECT_EQ(TracingExecutionPreference::Auto, *raytracer.tracingExecution());
    ASSERT_TRUE(raytracer.sampleStreamMode().has_value());
    EXPECT_EQ("gpu_sample_stream", *raytracer.sampleStreamMode());
  }

  TEST(RenderGraphRequest, AddsViewOverridesToResolvedIntent) {
    RenderGraphRequest request;
    RenderViewOverride wholeFrame;
    wholeFrame.selector = SceneSelector::all();
    wholeFrame.executor = RenderExecutorPreference::Rasterizer;

    RenderViewOverride tagged;
    tagged.selector = SceneSelector::tag("debug");
    tagged.viewMode = RenderViewMode::Wireframe;

    request.addViewOverride(wholeFrame).addViewOverride(tagged);

    const RenderIntent resolved = request.resolvedIntent();

    ASSERT_EQ(2u, resolved.viewOverrides.size());
    EXPECT_EQ(SceneSelector::Kind::All, resolved.viewOverrides[0].selector.kind);
    ASSERT_TRUE(resolved.viewOverrides[0].executor.has_value());
    EXPECT_EQ(RenderExecutorPreference::Rasterizer, *resolved.viewOverrides[0].executor);
    EXPECT_EQ(SceneSelector::Kind::Tag, resolved.viewOverrides[1].selector.kind);
    EXPECT_EQ("debug", resolved.viewOverrides[1].selector.value);
    ASSERT_TRUE(resolved.viewOverrides[1].viewMode.has_value());
    EXPECT_EQ(RenderViewMode::Wireframe, *resolved.viewOverrides[1].viewMode);
  }

  TEST(RenderGraphRequest, CompileUsesResolvedIntent) {
    RenderGraphRequest request;
    request.setExecutorOverride(RenderExecutorPreference::Rasterizer)
      .setViewModeOverride(RenderViewMode::Stencil);

    const RenderPlan plan = request.compile({16, 8, 1});

    ASSERT_NE(nullptr, plan.findPass("stencil_aov"));
    EXPECT_EQ(RenderExecutorKind::Rasterizer, plan.findPass("stencil_aov")->executor);
  }

  TEST(RenderGraphRequest, CompileUsesSceneAnalysis) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.enablePreviewShadows = true;

    RenderSceneAnalysis analysis;
    analysis.recordVisibleSurface();

    RenderGraphRequest request(intent);
    request.setSceneAnalysis(analysis);

    const RenderPlan plan = request.compile({16, 8, 1});

    EXPECT_EQ(nullptr, plan.findPass("raster_preview_shadows"));
  }
}
