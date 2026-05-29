#include <gtest/gtest.h>

#include "engine/raster/detail/OpenGLShadowSamplingPlan.h"
#include "engine/raster/detail/RasterShadowMaps.h"
#include "render/cameras/PinholeCamera.h"
#include "render/viewplanes/ViewPlane.h"

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

  std::shared_ptr<render::PinholeCamera> camera() {
    return std::make_shared<render::PinholeCamera>(Vector3d(0.0, 0.0, -5.0), Vector3d::null);
  }

  DirectionalShadowCascade cascade() {
    auto shadowCamera =
      std::make_shared<DirectionalShadowCamera>(Vector3d::null, Vector3d(0.0, 0.0, -1.0), 1.0);
    shadowCamera->setViewPlane(std::make_shared<render::ViewPlane>());
    shadowCamera->viewPlane()->setup(Matrix4d(), Recti(4, 4));

    auto depth = std::make_unique<Buffer<double>>(4, 4);
    depth->clear(std::numeric_limits<double>::infinity());
    return {std::move(shadowCamera), std::move(depth), 0.0, 10.0};
  }

  ShadowMaps shadowMaps(int filterRadius = 0, double slopeBias = 0.0) {
    std::vector<DirectionalShadowCascade> cascades;
    cascades.push_back(cascade());

    ShadowMaps maps;
    maps.add(DirectionalShadowMap(nullptr, camera(), std::move(cascades), 0.01, slopeBias,
                                  filterRadius, Rasterizer::ShadowFilterMode::PCF));
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

  TEST(OpenGLShadowSamplingPlan, RejectsFilteredDirectionalMaps) {
    ShadowMaps maps = shadowMaps(1);

    const OpenGLShadowSamplingPlan plan = OpenGLShadowSamplingPlan::from(&maps);

    EXPECT_FALSE(plan.enabled());
    EXPECT_NE(std::string::npos, plan.disabledReason().find("hard filtering"));
    EXPECT_NE(std::string::npos, plan.traceMessage().find("falls back"));
    EXPECT_NE(std::string::npos, plan.traceMessage().find("hard filtering"));
  }

  TEST(OpenGLShadowSamplingPlan, RejectsSlopeBiasedDirectionalMaps) {
    ShadowMaps maps = shadowMaps(0, 0.25);

    const OpenGLShadowSamplingPlan plan = OpenGLShadowSamplingPlan::from(&maps);

    EXPECT_FALSE(plan.enabled());
    EXPECT_NE(std::string::npos, plan.disabledReason().find("constant bias"));
  }
}
