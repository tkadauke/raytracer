#include <gtest/gtest.h>

#include "engine/graph/RasterPassState.h"
#include "engine/graph/RenderPlan.h"

#include <QJsonArray>
#include <QJsonObject>

namespace RasterPassStateTest {
  using namespace engine::graph;
  using Rasterizer = engine::raster::Rasterizer;

  TEST(RasterShadowPassState, SerializesPreviewDefaults) {
    const RasterShadowPassState state = RasterShadowPassState::previewDefaults();

    const QJsonObject json = state.toJson();
    const QJsonObject shadows = json.value("shadows").toObject();

    EXPECT_TRUE(shadows.value("enabled").toBool());
    EXPECT_EQ(4, shadows.value("cascadeCount").toInt());
    EXPECT_EQ(0.1, shadows.value("bias").toDouble());
    EXPECT_EQ(1, shadows.value("filterRadius").toInt());
  }

  TEST(RasterShadowPassState, AppliesImportedStateToRasterizer) {
    QJsonObject shadows;
    shadows["enabled"] = true;
    shadows["mapSize"] = 128;
    shadows["cascadeCount"] = 2;
    shadows["cascadeSplitLambda"] = 0.25;
    shadows["bias"] = 0.05;
    shadows["slopeBias"] = 0.02;
    shadows["filterRadius"] = 3;
    shadows["filterMode"] = "pcss";
    QJsonObject json;
    json["shadows"] = shadows;

    Rasterizer rasterizer(nullptr);
    RasterShadowPassState::fromJson(json).applyTo(rasterizer);

    EXPECT_TRUE(rasterizer.shadowMapsEnabled());
    EXPECT_EQ(128, rasterizer.shadowMapSize());
    EXPECT_EQ(2, rasterizer.shadowCascadeCount());
    EXPECT_EQ(0.25, rasterizer.shadowCascadeSplitLambda());
    EXPECT_EQ(0.05, rasterizer.shadowBias());
    EXPECT_EQ(0.02, rasterizer.shadowSlopeBias());
    EXPECT_EQ(3, rasterizer.shadowFilterRadius());
    EXPECT_EQ(Rasterizer::ShadowFilterMode::PCSS, rasterizer.shadowFilterMode());
  }

  TEST(RasterShadowPassState, WritesOnlyToRasterShadowPasses) {
    RenderPlan plan;
    RenderPassNode shadow;
    shadow.id = "raster_preview_shadows";
    shadow.kind = RenderPassKind::Shadow;
    shadow.executor = RenderExecutorKind::Rasterizer;
    plan.addPass(shadow);

    RenderPassNode beauty;
    beauty.id = "raster_beauty";
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = RenderExecutorKind::Rasterizer;
    plan.addPass(beauty);

    RasterShadowPassState state = RasterShadowPassState::previewDefaults();

    EXPECT_EQ(1u, state.writeToRasterShadowPasses(plan));
    ASSERT_NE(nullptr, plan.passes()[0].state);
    EXPECT_TRUE(RasterShadowPassState::fromPass(plan.passes()[0])->shadows().enabled());
    EXPECT_EQ(nullptr, plan.passes()[1].state);
  }

  TEST(RasterBeautyPassState, SerializesFocusedSubstates) {
    RasterBeautyPassState state;
    state.sampling().setMSAASamples(4);
    state.sampling().setMSAAShadingMode(Rasterizer::MSAAShadingMode::PerFragment);
    state.sampling().setPostProcessAA(Rasterizer::PostProcessAA::FXAA);
    state.framebuffer().setViewportRect(Recti(1, 2, 30, 40));
    state.shadows().setShadowMapsEnabled(true);
    state.shadows().setShadowMapSize(128);
    state.shadows().setShadowFilterMode(Rasterizer::ShadowFilterMode::PCSS);

    const QJsonObject json = state.toJson();
    const QJsonObject sampling = json.value("sampling").toObject();
    const QJsonObject framebuffer = json.value("framebuffer").toObject();
    const QJsonObject shadows = json.value("shadows").toObject();

    EXPECT_EQ(4, sampling.value("msaaSamples").toInt());
    EXPECT_EQ("per_fragment", sampling.value("msaaShadingMode").toString().toStdString());
    EXPECT_EQ("fxaa", sampling.value("postProcessAA").toString().toStdString());
    EXPECT_TRUE(framebuffer.value("viewport").isArray());
    EXPECT_TRUE(shadows.value("enabled").toBool());
    EXPECT_EQ(128, shadows.value("mapSize").toInt());
    EXPECT_EQ("pcss", shadows.value("filterMode").toString().toStdString());
  }

  TEST(RasterBeautyPassState, AppliesImportedStateToRasterizer) {
    QJsonObject execution;
    execution["queueSize"] = 7;
    QJsonObject geometry;
    geometry["lod"] = 3;
    geometry["cullMode"] = "back";
    QJsonObject sampling;
    sampling["msaaSamples"] = 4;
    sampling["msaaShadingMode"] = "per_fragment";
    sampling["postProcessAA"] = "smaa";
    QJsonObject framebuffer;
    framebuffer["colorWriteMask"] = "g";
    framebuffer["blending"] = true;
    framebuffer["blendSource"] = "constant_alpha";
    framebuffer["blendDestination"] = "one_minus_constant_alpha";
    framebuffer["blendOp"] = "max";
    framebuffer["blendConstantAlpha"] = 0.25;
    framebuffer["alphaTest"] = true;
    framebuffer["alphaFunc"] = "greater";
    framebuffer["alphaReference"] = 0.6;
    framebuffer["viewport"] = QJsonArray{4, 5, 20, 21};
    framebuffer["scissor"] = QJsonArray{6, 7, 18, 19};
    framebuffer["depthBias"] = -0.125;
    QJsonObject shadows;
    shadows["enabled"] = true;
    shadows["mapSize"] = 64;
    shadows["cascadeCount"] = 3;
    shadows["cascadeSplitLambda"] = 0.75;
    shadows["bias"] = 0.2;
    shadows["slopeBias"] = 0.03;
    shadows["filterRadius"] = 2;
    shadows["filterMode"] = "pcss";

    QJsonObject json;
    json["execution"] = execution;
    json["geometry"] = geometry;
    json["sampling"] = sampling;
    json["framebuffer"] = framebuffer;
    json["shadows"] = shadows;

    Rasterizer rasterizer(nullptr);
    RasterBeautyPassState::fromJson(json).applyTo(rasterizer);

    EXPECT_EQ(3, rasterizer.lod());
    EXPECT_EQ(7, rasterizer.queueSize());
    EXPECT_EQ(Rasterizer::CullMode::Back, rasterizer.cullMode());
    EXPECT_TRUE(rasterizer.hasCullModeOverride());
    EXPECT_EQ(4, rasterizer.msaaSamples());
    EXPECT_EQ(Rasterizer::MSAAShadingMode::PerFragment, rasterizer.msaaShadingMode());
    EXPECT_EQ(Rasterizer::PostProcessAA::SMAA, rasterizer.postProcessAA());
    EXPECT_EQ(Rasterizer::ColorWriteGreen, rasterizer.colorWriteMask());
    EXPECT_TRUE(rasterizer.blendingEnabled());
    EXPECT_EQ(Rasterizer::BlendFactor::ConstantAlpha, rasterizer.sourceBlendFactor());
    EXPECT_EQ(Rasterizer::BlendFactor::OneMinusConstantAlpha, rasterizer.destinationBlendFactor());
    EXPECT_EQ(Rasterizer::BlendOp::Max, rasterizer.blendOp());
    EXPECT_EQ(0.25, rasterizer.blendConstantAlpha());
    EXPECT_TRUE(rasterizer.alphaTestEnabled());
    EXPECT_EQ(Rasterizer::AlphaFunc::Greater, rasterizer.alphaFunc());
    EXPECT_EQ(0.6, rasterizer.alphaReference());
    EXPECT_TRUE(rasterizer.viewportEnabled());
    EXPECT_EQ(4, rasterizer.viewportRect().left());
    EXPECT_EQ(20, rasterizer.viewportRect().width());
    EXPECT_TRUE(rasterizer.scissorTestEnabled());
    EXPECT_EQ(6, rasterizer.scissorRect().left());
    EXPECT_EQ(18, rasterizer.scissorRect().width());
    EXPECT_EQ(-0.125, rasterizer.depthBias());
    EXPECT_TRUE(rasterizer.shadowMapsEnabled());
    EXPECT_EQ(64, rasterizer.shadowMapSize());
    EXPECT_EQ(3, rasterizer.shadowCascadeCount());
    EXPECT_EQ(0.75, rasterizer.shadowCascadeSplitLambda());
    EXPECT_EQ(0.2, rasterizer.shadowBias());
    EXPECT_EQ(0.03, rasterizer.shadowSlopeBias());
    EXPECT_EQ(2, rasterizer.shadowFilterRadius());
    EXPECT_EQ(Rasterizer::ShadowFilterMode::PCSS, rasterizer.shadowFilterMode());
  }

  TEST(RasterBeautyPassState, WritesOnlyToRasterBeautyPasses) {
    RenderPlan plan;
    RenderPassNode raster;
    raster.id = "raster_beauty";
    raster.kind = RenderPassKind::Beauty;
    raster.executor = RenderExecutorKind::Rasterizer;
    plan.addPass(raster);

    RenderPassNode raytrace;
    raytrace.id = "raytrace_beauty";
    raytrace.kind = RenderPassKind::Beauty;
    raytrace.executor = RenderExecutorKind::Raytracer;
    plan.addPass(raytrace);

    RasterBeautyPassState state;
    state.sampling().setMSAASamples(2);

    EXPECT_EQ(1u, state.writeToRasterBeautyPasses(plan));
    ASSERT_NE(nullptr, plan.passes()[0].state);
    EXPECT_EQ(2, RasterBeautyPassState::fromPass(plan.passes()[0])->sampling().msaaSamples());
    EXPECT_EQ(nullptr, plan.passes()[1].state);
  }

  TEST(RasterBeautyPassState, RejectsUnknownFieldsDuringImport) {
    QJsonObject sampling;
    sampling["msaaSamples"] = 4;
    sampling["surprise"] = true;
    QJsonObject json;
    json["sampling"] = sampling;

    EXPECT_THROW(RasterBeautyPassState::fromJson(json), std::runtime_error);
  }
}
