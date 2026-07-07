#include <gtest/gtest.h>

#include "engine/raster/detail/OpenGLShadowSamplingPlan.h"
#include "engine/raster/detail/RasterShadowMaps.h"
#include "render/cameras/PinholeCamera.h"
#include "render/lights/DirectionalLight.h"
#include "render/viewplanes/ViewPlane.h"
#include "render/primitives/Scene.h"
#include "test/helpers/CameraTestHelper.h"

#include <limits>
#include <memory>
#include <string>

namespace OpenGLShadowSamplingPlanTest {
  using engine::raster::Rasterizer;
  using engine::raster::detail::DirectionalShadowCamera;
  using engine::raster::detail::DirectionalShadowCascade;
  using engine::raster::detail::DirectionalShadowMap;
  using engine::raster::detail::OpenGLShadowSamplingPlan;
  using engine::raster::detail::ShadowMaps;
  using render::DirectionalLight;
  using test::helpers::standardCamera;

  DirectionalShadowCascade cascade() {
    auto shadowCamera =
      std::make_shared<DirectionalShadowCamera>(Vector3d::null, Vector3d(0.0, 0.0, -1.0), 1.0);
    shadowCamera->setViewPlane(std::make_shared<render::ViewPlane>());
    shadowCamera->viewPlane()->setup(Matrix4d(), Recti(4, 4));

    auto depth = std::make_unique<Buffer<double>>(4, 4);
    depth->clear(std::numeric_limits<double>::infinity());
    return {std::move(shadowCamera), std::move(depth), 0.0, 10.0};
  }

  ShadowMaps
  shadowMaps(const render::Light* light = nullptr, int filterRadius = 0, double slopeBias = 0.0,
             Rasterizer::ShadowFilterMode filterMode = Rasterizer::ShadowFilterMode::PCF) {
    std::vector<DirectionalShadowCascade> cascades;
    cascades.push_back(cascade());

    ShadowMaps maps;
    maps.add(DirectionalShadowMap(light, standardCamera(), std::move(cascades), 0.01, slopeBias,
                                  filterRadius, filterMode));
    return maps;
  }

  TEST(OpenGLShadowSamplingPlan, DisablesEmptyShadowMaps) {
    ShadowMaps maps;

    const OpenGLShadowSamplingPlan plan = OpenGLShadowSamplingPlan::from(&maps);

    EXPECT_FALSE(plan.enabled());
    EXPECT_NE(std::string::npos, plan.disabledReason().find("no directional shadow maps"));
  }

  TEST(OpenGLShadowSamplingPlan, EnablesSingleHardDirectionalCascade) {
    ShadowMaps maps = shadowMaps();

    const OpenGLShadowSamplingPlan plan = OpenGLShadowSamplingPlan::from(&maps);

    ASSERT_TRUE(plan.enabled());
    EXPECT_EQ(&maps.directionalMaps().front(), plan.shadowMap());
    EXPECT_EQ(&maps.directionalMaps().front().cascades().front(), plan.cascade());
    EXPECT_TRUE(plan.disabledReason().empty());
    EXPECT_NE(std::string::npos, plan.traceMessage().find("eligible for shader-side binding"));
    EXPECT_NE(std::string::npos, plan.traceMessage().find("CPU-prepared shadow visibility"));
  }

  TEST(OpenGLShadowSamplingPlan, EnablesSmallPcfDirectionalMaps) {
    ShadowMaps maps = shadowMaps(nullptr, 2);

    const OpenGLShadowSamplingPlan plan = OpenGLShadowSamplingPlan::from(&maps);

    EXPECT_TRUE(plan.enabled());
    EXPECT_TRUE(plan.disabledReason().empty());
  }

  TEST(OpenGLShadowSamplingPlan, RejectsWidePcfDirectionalMaps) {
    ShadowMaps maps = shadowMaps(nullptr, 5);

    const OpenGLShadowSamplingPlan plan = OpenGLShadowSamplingPlan::from(&maps);

    EXPECT_FALSE(plan.enabled());
    EXPECT_NE(std::string::npos, plan.disabledReason().find("radius up to 4"));
  }

  TEST(OpenGLShadowSamplingPlan, RejectsPcssDirectionalMaps) {
    ShadowMaps maps = shadowMaps(nullptr, 1, 0.0, Rasterizer::ShadowFilterMode::PCSS);

    const OpenGLShadowSamplingPlan plan = OpenGLShadowSamplingPlan::from(&maps);

    EXPECT_FALSE(plan.enabled());
    EXPECT_NE(std::string::npos, plan.disabledReason().find("PCSS"));
  }

  TEST(OpenGLShadowSamplingPlan, RejectsSlopeBiasedDirectionalMaps) {
    ShadowMaps maps = shadowMaps(nullptr, 0, 0.25);

    const OpenGLShadowSamplingPlan plan = OpenGLShadowSamplingPlan::from(&maps);

    EXPECT_FALSE(plan.enabled());
    EXPECT_NE(std::string::npos, plan.disabledReason().find("constant bias"));
  }

  TEST(OpenGLShadowSamplingPlan, AcceptsOnlyMatchingSingleLightForShaderLighting) {
    auto light = std::make_shared<DirectionalLight>(Vector3d(0.0, 0.0, -1.0), Colord::white());
    auto scene = std::make_shared<render::Scene>();
    scene->addLight(light);
    ShadowMaps maps = shadowMaps(light.get());
    const OpenGLShadowSamplingPlan plan = OpenGLShadowSamplingPlan::from(&maps);

    EXPECT_TRUE(plan.canShadeSceneDirectLighting(scene.get()));
    EXPECT_TRUE(plan.shaderLightingDisabledReason(scene.get()).empty());
    EXPECT_NE(std::string::npos, plan.traceMessage(scene.get()).find("uses shader-side binding"));
  }

  TEST(OpenGLShadowSamplingPlan, RejectsMismatchedSceneLightsForShaderLighting) {
    auto shadowLight =
      std::make_shared<DirectionalLight>(Vector3d(0.0, 0.0, -1.0), Colord::white());
    auto extraLight = std::make_shared<DirectionalLight>(Vector3d(0.0, -1.0, 0.0), Colord::white());
    auto scene = std::make_shared<render::Scene>();
    scene->addLight(shadowLight);
    scene->addLight(extraLight);
    ShadowMaps maps = shadowMaps(shadowLight.get());
    const OpenGLShadowSamplingPlan plan = OpenGLShadowSamplingPlan::from(&maps);

    EXPECT_FALSE(plan.canShadeSceneDirectLighting(scene.get()));
    EXPECT_NE(std::string::npos,
              plan.shaderLightingDisabledReason(scene.get()).find("one scene light"));
    EXPECT_NE(std::string::npos, plan.traceMessage(scene.get()).find("falls back"));

    auto otherScene = std::make_shared<render::Scene>();
    otherScene->addLight(extraLight);
    EXPECT_FALSE(plan.canShadeSceneDirectLighting(otherScene.get()));
    EXPECT_NE(std::string::npos,
              plan.shaderLightingDisabledReason(otherScene.get()).find("own the only scene light"));
  }
}
