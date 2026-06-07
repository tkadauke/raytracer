#include <gtest/gtest.h>

#include "engine/graph/RenderSceneAnalysis.h"

namespace RenderSceneAnalysisTest {
  using namespace engine::graph;

  TEST(RenderSceneAnalysis, StartsAsKnownEmptyScene) {
    RenderSceneAnalysis analysis;

    EXPECT_TRUE(analysis.hasKnownVisibleSurfaceCount());
    EXPECT_TRUE(analysis.hasKnownVisibleLightCount());
    EXPECT_EQ(0u, analysis.visibleSurfaceCount());
    EXPECT_EQ(0u, analysis.visibleLightCount());
    EXPECT_FALSE(analysis.hasVisibleSurfaces());
    EXPECT_FALSE(analysis.hasVisibleLights());
  }

  TEST(RenderSceneAnalysis, UnknownScenePreservesConservativeFeatureExpansion) {
    const RenderSceneAnalysis analysis = RenderSceneAnalysis::unknownScene();

    EXPECT_FALSE(analysis.hasKnownVisibleSurfaceCount());
    EXPECT_FALSE(analysis.hasKnownVisibleLightCount());
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
    EXPECT_TRUE(analysis.portalReceiverSurfaces()[0].receiverVisibleInPrimaryView);
    EXPECT_EQ("mirror-panel", analysis.planarMirrorSurfaces()[0].surfaceId);
    EXPECT_EQ("Mirror Panel", analysis.planarMirrorSurfaces()[0].surfaceName);
    EXPECT_EQ(Vector3d(0.0, 1.0, 0.0), analysis.planarMirrorSurfaces()[0].planeNormal);
  }

  TEST(RenderSceneAnalysis, RecordsOffscreenSceneSurfaceMarkers) {
    RenderSceneAnalysis analysis;

    analysis.recordPortalReceiverSurface("portal-panel", "Portal Panel", Matrix4d(), Matrix4d(),
                                         false);
    analysis.recordPlanarMirrorSurface("mirror-panel", "Mirror Panel", Vector3d::null,
                                       Vector3d(0.0, 1.0, 0.0), false);

    ASSERT_EQ(1u, analysis.portalReceiverSurfaceCount());
    ASSERT_EQ(1u, analysis.planarMirrorSurfaceCount());
    EXPECT_FALSE(analysis.portalReceiverSurfaces()[0].receiverVisibleInPrimaryView);
    EXPECT_FALSE(analysis.planarMirrorSurfaces()[0].receiverVisibleInPrimaryView);
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
