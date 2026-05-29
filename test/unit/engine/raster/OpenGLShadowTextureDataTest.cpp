#include <gtest/gtest.h>

#include "engine/raster/detail/OpenGLShadowSamplingPlan.h"
#include "engine/raster/detail/OpenGLShadowTextureData.h"
#include "engine/raster/detail/RasterShadowMaps.h"
#include "render/cameras/PinholeCamera.h"
#include "render/viewplanes/ViewPlane.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <string>

namespace OpenGLShadowTextureDataTest {
  using engine::raster::Rasterizer;
  using engine::raster::detail::DirectionalShadowCamera;
  using engine::raster::detail::DirectionalShadowCascade;
  using engine::raster::detail::DirectionalShadowMap;
  using engine::raster::detail::OpenGLShadowSamplingPlan;
  using engine::raster::detail::OpenGLShadowTextureData;
  using engine::raster::detail::ShadowMaps;

  constexpr int kWidth = 2;
  constexpr int kHeight = 2;
  constexpr int kFilterRadius = 2;

  std::shared_ptr<render::PinholeCamera> camera() {
    return std::make_shared<render::PinholeCamera>(Vector3d(0.0, 0.0, -5.0), Vector3d::null);
  }

  constexpr double kShadowBias = 0.01;

  std::shared_ptr<DirectionalShadowCamera> shadowCamera() {
    return std::make_shared<DirectionalShadowCamera>(Vector3d(1.0, 2.0, 3.0),
                                                     Vector3d(0.0, 0.0, -1.0), 4.0);
  }

  DirectionalShadowCascade cascade() {
    auto shadowCamera = OpenGLShadowTextureDataTest::shadowCamera();
    shadowCamera->setViewPlane(std::make_shared<render::ViewPlane>());
    shadowCamera->viewPlane()->setup(Matrix4d(), Recti(kWidth, kHeight));

    auto depth = std::make_unique<Buffer<double>>(kWidth, kHeight);
    (*depth)[0][0] = 2.0;
    (*depth)[0][1] = std::numeric_limits<double>::infinity();
    (*depth)[1][0] = 4.0;
    (*depth)[1][1] = 6.0;
    return {std::move(shadowCamera), std::move(depth), 0.0, 10.0};
  }

  ShadowMaps shadowMaps() {
    std::vector<DirectionalShadowCascade> cascades;
    cascades.push_back(cascade());

    ShadowMaps maps;
    maps.add(DirectionalShadowMap(nullptr, camera(), std::move(cascades), kShadowBias, 0.0,
                                  kFilterRadius, Rasterizer::ShadowFilterMode::PCF));
    return maps;
  }

  std::size_t pixelOffset(int x, int y) {
    return static_cast<std::size_t>((y * kWidth + x) * 4);
  }

  TEST(OpenGLShadowTextureData, LeavesDisabledPlansEmpty) {
    const OpenGLShadowSamplingPlan plan = OpenGLShadowSamplingPlan::from(nullptr);

    const OpenGLShadowTextureData data = OpenGLShadowTextureData::from(plan);

    EXPECT_FALSE(data.enabled());
    EXPECT_EQ(0, data.width());
    EXPECT_EQ(0, data.height());
    EXPECT_TRUE(data.rgbaPixels().empty());
  }

  TEST(OpenGLShadowTextureData, NormalizesFiniteDepthAndKeepsInfiniteDepthLit) {
    ShadowMaps maps = shadowMaps();
    const OpenGLShadowSamplingPlan plan = OpenGLShadowSamplingPlan::from(&maps);

    const OpenGLShadowTextureData data = OpenGLShadowTextureData::from(plan);

    ASSERT_TRUE(data.enabled());
    EXPECT_EQ(kWidth, data.width());
    EXPECT_EQ(kHeight, data.height());
    EXPECT_DOUBLE_EQ(7.0, data.depthScale());
    EXPECT_DOUBLE_EQ(kShadowBias, data.bias());
    EXPECT_EQ(kFilterRadius, data.filterRadius());
    EXPECT_EQ(Vector3d(1.0, 2.0, -5.0), data.origin());
    EXPECT_EQ(Vector3d::right(), data.right());
    EXPECT_EQ(Vector3d::up(), data.up());
    EXPECT_EQ(Vector3d::forward(), data.forward());
    EXPECT_DOUBLE_EQ(4.0, data.halfExtent());
    EXPECT_EQ(static_cast<std::size_t>(kWidth * kHeight * 4 * sizeof(float)),
              data.uploadByteSize());
    EXPECT_NE(std::string::npos, data.traceMessage().find("2x2"));
    EXPECT_NE(std::string::npos, data.traceMessage().find("64 upload bytes"));
    EXPECT_NE(std::string::npos, data.traceMessage().find("shader-side binding"));
    ASSERT_EQ(static_cast<std::size_t>(kWidth * kHeight * 4), data.rgbaPixels().size());
    EXPECT_NEAR(2.0 / 7.0, data.rgbaPixels()[pixelOffset(0, 0)], 0.000001);
    EXPECT_FLOAT_EQ(1.0f, data.rgbaPixels()[pixelOffset(1, 0)]);
    EXPECT_NEAR(6.0 / 7.0, data.rgbaPixels()[pixelOffset(1, 1)], 0.000001);
    EXPECT_FLOAT_EQ(1.0f, data.rgbaPixels()[pixelOffset(1, 1) + 3]);
  }
}
