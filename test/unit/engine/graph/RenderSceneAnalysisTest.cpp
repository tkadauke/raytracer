#include <gtest/gtest.h>

#include "engine/graph/RenderSceneAnalysis.h"

namespace RenderSceneAnalysisTest {
  using namespace engine::graph;

  TEST(RenderSceneAnalysis, StartsAsKnownEmptyScene) {
    RenderSceneAnalysis analysis;

    EXPECT_TRUE(analysis.hasKnownVisibleSurfaceCount());
    EXPECT_TRUE(analysis.hasKnownVisibleLightCount());
    EXPECT_TRUE(analysis.hasKnownSelectableSubsets());
    EXPECT_EQ(0u, analysis.visibleSurfaceCount());
    EXPECT_EQ(0u, analysis.visibleLightCount());
    EXPECT_FALSE(analysis.hasVisibleSurfaces());
    EXPECT_FALSE(analysis.hasVisibleLights());
  }

  TEST(RenderSceneAnalysis, UnknownScenePreservesConservativeFeatureExpansion) {
    const RenderSceneAnalysis analysis = RenderSceneAnalysis::unknownScene();

    EXPECT_FALSE(analysis.hasKnownVisibleSurfaceCount());
    EXPECT_FALSE(analysis.hasKnownVisibleLightCount());
    EXPECT_FALSE(analysis.hasKnownSelectableSubsets());
    EXPECT_EQ(0u, analysis.visibleSurfaceCount());
    EXPECT_EQ(0u, analysis.visibleLightCount());
    EXPECT_TRUE(analysis.hasVisibleSurfaces());
    EXPECT_TRUE(analysis.hasVisibleLights());
  }

  TEST(RenderSceneAnalysis, RecordsVisibleSurfacesAndLights) {
    RenderSceneAnalysis analysis;

    analysis.recordVisibleSurface();
    analysis.recordVisibleSurface();
    analysis.recordVisibleLight();

    EXPECT_EQ(2u, analysis.visibleSurfaceCount());
    EXPECT_EQ(1u, analysis.visibleLightCount());
    EXPECT_TRUE(analysis.hasVisibleSurfaces());
    EXPECT_TRUE(analysis.hasVisibleLights());
  }

  TEST(RenderSceneAnalysis, RecordsSceneSurfaceMarkers) {
    RenderSceneAnalysis analysis;

    analysis.recordPortalReceiverSurface("portal-panel", "Portal Panel",
                                         Matrix4d::translate(1.0, 2.0, 3.0),
                                         Matrix4d::translate(10.0, 0.0, 0.0));
    analysis.recordPlanarMirrorSurface("mirror-panel", "Mirror Panel", Vector3d(0.0, 2.0, 0.0),
                                       Vector3d(0.0, 2.0, 0.0));

    ASSERT_EQ(1u, analysis.portalReceiverSurfaceCount());
    ASSERT_EQ(1u, analysis.planarMirrorSurfaceCount());
    EXPECT_EQ("portal-panel", analysis.portalReceiverSurfaces()[0].surfaceId);
    EXPECT_EQ("Portal Panel", analysis.portalReceiverSurfaces()[0].surfaceName);
    EXPECT_EQ(Vector3d(1.0, 2.0, 3.0), analysis.portalReceiverSurfaces()[0].planePoint);
    EXPECT_EQ("mirror-panel", analysis.planarMirrorSurfaces()[0].surfaceId);
    EXPECT_EQ("Mirror Panel", analysis.planarMirrorSurfaces()[0].surfaceName);
    EXPECT_EQ(Vector3d(0.0, 1.0, 0.0), analysis.planarMirrorSurfaces()[0].planeNormal);
  }

  TEST(RenderSceneAnalysis, RecordsSelectableObjectTagAndLayerSubsets) {
    RenderSceneAnalysis analysis;

    analysis.recordSelectableObject("sphere-1", "Hero Sphere", {"hero", "matte"},
                                    {"foreground"}, "Hero Sphere");

    const auto& subsets = analysis.selectableSubsets();
    ASSERT_EQ(5u, subsets.size());
    EXPECT_EQ("object_id:sphere-1", subsets[0].id);
    EXPECT_EQ("Hero Sphere", subsets[0].label);
    EXPECT_EQ(SceneSelector::Kind::ObjectId, subsets[0].selector.kind);
    EXPECT_EQ("sphere-1", subsets[0].selector.value);
    EXPECT_EQ(1u, subsets[0].elementCount);
    EXPECT_EQ("tag:hero", subsets[2].id);
    EXPECT_EQ("Tag: hero", subsets[2].label);
    EXPECT_EQ("layer:foreground", subsets[4].id);
    EXPECT_EQ("Layer: foreground", subsets[4].label);
  }

  TEST(RenderSceneAnalysis, MatchesAggregateTagAndLayerSelectors) {
    RenderSceneAnalysis analysis;
    analysis.recordSelectableObject("sphere-1", "Hero Sphere", {"hero"}, {"foreground"});
    analysis.recordSelectableObject("light-1", "Key Light", {"hero"}, {"foreground"});

    const auto tagMatch = analysis.matchSelector(SceneSelector::tag("hero"));
    ASSERT_TRUE(tagMatch.matched());
    EXPECT_EQ("tag:hero", tagMatch.subset->id);
    EXPECT_EQ(2u, tagMatch.subset->elementCount);

    const auto layerMatch = analysis.matchSelector(SceneSelector::layer("foreground"));
    ASSERT_TRUE(layerMatch.matched());
    EXPECT_EQ("layer:foreground", layerMatch.subset->id);
    EXPECT_EQ(2u, layerMatch.subset->elementCount);
  }

  TEST(RenderSceneAnalysis, ReportsMissingAndAmbiguousSelectors) {
    RenderSceneAnalysis analysis;
    analysis.recordSelectableObject("sphere-1", "Duplicate", {}, {});
    analysis.recordSelectableObject("sphere-2", "Duplicate", {}, {});

    const auto missing = analysis.matchSelector(SceneSelector::tag("missing"));
    EXPECT_EQ(RenderSceneAnalysis::SelectorMatchStatus::Missing, missing.status);
    EXPECT_FALSE(missing.matched());

    const auto ambiguous = analysis.matchSelector(SceneSelector::objectName("Duplicate"));
    EXPECT_EQ(RenderSceneAnalysis::SelectorMatchStatus::Ambiguous, ambiguous.status);
    EXPECT_FALSE(ambiguous.matched());
    ASSERT_EQ(1u, ambiguous.candidates.size());
    EXPECT_EQ(2u, ambiguous.candidates.front()->elementCount);
  }

  TEST(RenderSceneAnalysis, RasterPreviewShadowsNeedRasterIntentSurfacesAndLights) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.enablePreviewShadows = true;

    RenderSceneAnalysis empty;
    EXPECT_FALSE(empty.shouldCompileRasterPreviewShadows(RenderExecutorKind::Rasterizer, intent));

    RenderSceneAnalysis surfacesOnly;
    surfacesOnly.recordVisibleSurface();
    EXPECT_FALSE(
      surfacesOnly.shouldCompileRasterPreviewShadows(RenderExecutorKind::Rasterizer, intent));

    RenderSceneAnalysis litScene;
    litScene.recordVisibleSurface();
    litScene.recordVisibleLight();
    EXPECT_TRUE(litScene.shouldCompileRasterPreviewShadows(RenderExecutorKind::Rasterizer, intent));
    EXPECT_FALSE(litScene.shouldCompileRasterPreviewShadows(RenderExecutorKind::Raytracer, intent));
  }
}
