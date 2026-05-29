#include <gtest/gtest.h>

#include "engine/graph/RasterPassState.h"
#include "engine/graph/RenderPlan.h"
#include "engine/raster/OpenGLRasterizer.h"

#include <QJsonArray>
#include <QJsonObject>

#include <stdexcept>
#include <string>

namespace RasterPassStateTest {
  using namespace engine::graph;
  using Rasterizer = engine::raster::Rasterizer;

  void expectOpenGLUnsupported(const RasterBeautyPassState& state, const std::string& feature) {
    engine::raster::OpenGLRasterizer rasterizer(nullptr);

    try {
      state.applyTo(rasterizer);
      FAIL() << "expected unsupported OpenGL raster state";
    } catch (const std::runtime_error& error) {
      const std::string message = error.what();
      EXPECT_NE(message.find("OpenGL raster backend"), std::string::npos);
      EXPECT_NE(message.find(feature), std::string::npos);
    }
  }

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

  TEST(RasterShadowPassState, BuildsShadowMapResourceDescriptor) {
    RasterShadowPassState state = RasterShadowPassState::previewDefaults();
    state.shadows().setShadowMapSize(128);

    const RenderResourceDescriptor descriptor =
      state.shadows().resourceDescriptor("preview_shadow_map", "Preview shadow map");

    EXPECT_EQ("preview_shadow_map", descriptor.id);
    EXPECT_EQ("Preview shadow map", descriptor.name);
    EXPECT_EQ(RenderResourceType::ShadowMap, descriptor.type);
    EXPECT_EQ(RenderResourceFormat::DepthDouble, descriptor.format);
    EXPECT_EQ(128, descriptor.width);
    EXPECT_EQ(128, descriptor.height);
    EXPECT_EQ(1, descriptor.sampleCount);
    EXPECT_EQ(RenderResourceDomain::CPU, descriptor.domain);
    EXPECT_EQ(RenderResourceLifetime::PersistentCache, descriptor.lifetime);
  }

  TEST(RasterShadowPassState, WritesOnlyToRasterShadowPasses) {
    RenderPlan plan;
    RenderPassNode shadow;
    shadow.id = "raster_preview_shadows";
    shadow.kind = RenderPassKind::Shadow;
    shadow.executor = RenderExecutorKind::Rasterizer;
    shadow.writes.push_back({"preview_shadow_map"});
    RenderResourceDescriptor shadowMap;
    shadowMap.id = "preview_shadow_map";
    shadowMap.type = RenderResourceType::ShadowMap;
    plan.addResource(shadowMap);
    plan.addPass(shadow);

    RenderPassNode beauty;
    beauty.id = "raster_beauty";
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = RenderExecutorKind::Rasterizer;
    plan.addPass(beauty);

    RasterShadowPassState state = RasterShadowPassState::previewDefaults();
    state.shadows().setShadowMapSize(128);

    EXPECT_EQ(1u, state.writeToRasterShadowPasses(plan));
    ASSERT_NE(nullptr, plan.passes()[0].state);
    EXPECT_TRUE(RasterShadowPassState::fromPass(plan.passes()[0])->shadows().enabled());
    EXPECT_EQ(nullptr, plan.passes()[1].state);
    ASSERT_NE(nullptr, plan.findResource("preview_shadow_map"));
    EXPECT_EQ(128, plan.findResource("preview_shadow_map")->width);
    EXPECT_EQ(128, plan.findResource("preview_shadow_map")->height);
    EXPECT_EQ(RenderResourceFormat::DepthDouble, plan.findResource("preview_shadow_map")->format);
    EXPECT_EQ(RenderResourceLifetime::PersistentCache,
              plan.findResource("preview_shadow_map")->lifetime);
  }

  TEST(RasterVisibilityPassState, SerializesGeometryState) {
    RasterVisibilityPassState state;
    state.geometry().setLod(3);
    state.geometry().setCullMode(Rasterizer::CullMode::Back);

    const QJsonObject json = state.toJson();
    const QJsonObject geometry = json.value("geometry").toObject();

    EXPECT_EQ(3, geometry.value("lod").toInt());
    EXPECT_EQ("back", geometry.value("cullMode").toString().toStdString());

    const auto decoded = RenderPassState::fromJson(
      RenderPassKind::Visibility, RenderExecutorKind::Rasterizer, json, "parameters");
    ASSERT_NE(nullptr, decoded);
    const auto* visibility = decoded->asRasterVisibilityPassState();
    ASSERT_NE(nullptr, visibility);
    EXPECT_EQ(3, visibility->geometry().lod());
  }

  TEST(RasterVisibilityPassState, WritesToVisibilityPass) {
    RenderPassNode pass;
    pass.id = "raster_visibility";
    pass.kind = RenderPassKind::Visibility;
    pass.executor = RenderExecutorKind::Rasterizer;

    RasterVisibilityPassState state;
    state.geometry().setLod(2);
    state.writeTo(pass);

    const auto* decoded = RasterVisibilityPassState::fromPass(pass);
    ASSERT_NE(nullptr, decoded);
    EXPECT_EQ(2, decoded->geometry().lod());
  }

  TEST(RasterBeautyPassState, SerializesFocusedSubstates) {
    RasterBeautyPassState state;
    state.execution().setBackend(engine::raster::RasterBackend::openGL());
    state.sampling().setMSAASamples(4);
    state.sampling().setMSAAShadingMode(Rasterizer::MSAAShadingMode::PerFragment);
    state.sampling().setPostProcessAA(Rasterizer::PostProcessAA::FXAA);
    state.framebuffer().setViewportRect(Recti(1, 2, 30, 40));
    state.framebuffer().setColorLoadOp(Rasterizer::AttachmentLoadOp::Load);
    state.framebuffer().setColorStoreOp(Rasterizer::AttachmentStoreOp::Discard);
    state.framebuffer().setDepthFunc(Rasterizer::DepthFunc::GreaterEqual);
    state.framebuffer().setDepthClearValue(4.5);
    state.framebuffer().setDepthStoreOp(Rasterizer::AttachmentStoreOp::Discard);
    state.framebuffer().setDepthWriteEnabled(false);
    state.framebuffer().configureStencilWritePass(0x7f);
    state.shadows().setShadowMapsEnabled(true);
    state.shadows().setShadowMapSize(128);
    state.shadows().setShadowFilterMode(Rasterizer::ShadowFilterMode::PCSS);

    const QJsonObject json = state.toJson();
    const QJsonObject execution = json.value("execution").toObject();
    const QJsonObject sampling = json.value("sampling").toObject();
    const QJsonObject framebuffer = json.value("framebuffer").toObject();
    const QJsonObject shadows = json.value("shadows").toObject();

    EXPECT_EQ("opengl", execution.value("backend").toString().toStdString());
    EXPECT_EQ(4, sampling.value("msaaSamples").toInt());
    EXPECT_EQ("per_fragment", sampling.value("msaaShadingMode").toString().toStdString());
    EXPECT_EQ("fxaa", sampling.value("postProcessAA").toString().toStdString());
    EXPECT_TRUE(framebuffer.value("viewport").isArray());
    EXPECT_EQ("load", framebuffer.value("colorLoadOp").toString().toStdString());
    EXPECT_EQ("discard", framebuffer.value("colorStoreOp").toString().toStdString());
    EXPECT_EQ("greater_equal", framebuffer.value("depthFunc").toString().toStdString());
    EXPECT_EQ(4.5, framebuffer.value("depthClearValue").toDouble());
    EXPECT_EQ("discard", framebuffer.value("depthStoreOp").toString().toStdString());
    EXPECT_FALSE(framebuffer.value("depthWrite").toBool(true));
    EXPECT_TRUE(framebuffer.value("stencilTest").toBool());
    EXPECT_EQ(0x7f, framebuffer.value("stencilReference").toInt());
    EXPECT_EQ("replace", framebuffer.value("stencilPassOp").toString().toStdString());
    EXPECT_TRUE(shadows.value("enabled").toBool());
    EXPECT_EQ(128, shadows.value("mapSize").toInt());
    EXPECT_EQ("pcss", shadows.value("filterMode").toString().toStdString());
  }

  TEST(RasterBeautyPassState, SerializesPerSampleMSAAShadingWhenMSAAEnabled) {
    RasterBeautyPassState state;
    state.sampling().setMSAASamples(4);
    state.sampling().setMSAAShadingMode(Rasterizer::MSAAShadingMode::PerSample);

    const QJsonObject sampling = state.toJson().value("sampling").toObject();

    EXPECT_EQ(4, sampling.value("msaaSamples").toInt());
    EXPECT_EQ("per_sample", sampling.value("msaaShadingMode").toString().toStdString());
  }

  TEST(RasterBeautyPassState, AppliesSupportedStateToOpenGLRasterizer) {
    RasterBeautyPassState state;
    state.geometry().setLod(3);
    state.geometry().setCullMode(Rasterizer::CullMode::Front);
    state.sampling().setMSAASamples(4);
    state.framebuffer().setViewportRect(Recti(4, 5, 20, 21));
    state.framebuffer().setScissorRect(Recti(6, 7, 18, 19));
    state.framebuffer().setColorStoreOp(Rasterizer::AttachmentStoreOp::Discard);
    state.framebuffer().setDepthFunc(Rasterizer::DepthFunc::Greater);
    state.framebuffer().setDepthBias(-0.125);
    state.framebuffer().setDepthClearValue(6.25);
    state.framebuffer().setDepthStoreOp(Rasterizer::AttachmentStoreOp::Discard);
    state.framebuffer().setDepthWriteEnabled(false);
    state.framebuffer().setColorWriteMask(Rasterizer::ColorWriteRed);
    state.framebuffer().setBlendingEnabled(true);
    state.framebuffer().setBlendFactors(Rasterizer::BlendFactor::ConstantAlpha,
                                        Rasterizer::BlendFactor::OneMinusConstantAlpha);
    state.framebuffer().setBlendOp(Rasterizer::BlendOp::Max);
    state.framebuffer().setBlendConstant(Colord(0.2, 0.3, 0.4), 0.5);
    state.framebuffer().setAlphaTestEnabled(true);
    state.framebuffer().setAlphaFunc(Rasterizer::AlphaFunc::Greater, 0.6);
    state.framebuffer().configureStencilWritePass(0x7f);
    engine::raster::OpenGLRasterizer rasterizer(nullptr);

    state.applyTo(rasterizer);

    EXPECT_EQ(3, rasterizer.lod());
    EXPECT_EQ(4, rasterizer.msaaSamples());
    EXPECT_EQ(Rasterizer::MSAAShadingMode::PerSample, rasterizer.msaaShadingMode());
    EXPECT_TRUE(rasterizer.hasCullModeOverride());
    EXPECT_EQ(Rasterizer::CullMode::Front, rasterizer.cullMode());
    EXPECT_TRUE(rasterizer.viewportEnabled());
    EXPECT_EQ(4, rasterizer.viewportRect().left());
    EXPECT_EQ(20, rasterizer.viewportRect().width());
    EXPECT_TRUE(rasterizer.scissorTestEnabled());
    EXPECT_EQ(6, rasterizer.scissorRect().left());
    EXPECT_EQ(18, rasterizer.scissorRect().width());
    EXPECT_EQ(Rasterizer::AttachmentStoreOp::Discard, rasterizer.colorStoreOp());
    EXPECT_EQ(Rasterizer::DepthFunc::Greater, rasterizer.depthFunc());
    EXPECT_EQ(-0.125, rasterizer.depthBias());
    EXPECT_EQ(6.25, rasterizer.depthClearValue());
    EXPECT_EQ(Rasterizer::AttachmentStoreOp::Discard, rasterizer.depthStoreOp());
    EXPECT_FALSE(rasterizer.depthWriteEnabled());
    EXPECT_EQ(Rasterizer::ColorWriteRed, rasterizer.colorWriteMask());
    EXPECT_TRUE(rasterizer.blendingEnabled());
    EXPECT_EQ(Rasterizer::BlendFactor::ConstantAlpha, rasterizer.sourceBlendFactor());
    EXPECT_EQ(Rasterizer::BlendFactor::OneMinusConstantAlpha, rasterizer.destinationBlendFactor());
    EXPECT_EQ(Rasterizer::BlendOp::Max, rasterizer.blendOp());
    EXPECT_EQ(Colord(0.2, 0.3, 0.4), rasterizer.blendConstantColor());
    EXPECT_EQ(0.5, rasterizer.blendConstantAlpha());
    EXPECT_TRUE(rasterizer.alphaTestEnabled());
    EXPECT_EQ(Rasterizer::AlphaFunc::Greater, rasterizer.alphaFunc());
    EXPECT_EQ(0.6, rasterizer.alphaReference());
    EXPECT_TRUE(rasterizer.stencilTestEnabled());
    EXPECT_EQ(Rasterizer::StencilFunc::Always, rasterizer.stencilFunc());
    EXPECT_EQ(0x7f, rasterizer.stencilReference());
    EXPECT_EQ(Rasterizer::StencilOp::Replace, rasterizer.stencilPassOp());
  }

  TEST(RasterBeautyPassState, RejectsUnsupportedOpenGLPostProcessAA) {
    RasterBeautyPassState state;
    state.sampling().setPostProcessAA(Rasterizer::PostProcessAA::FXAA);

    expectOpenGLUnsupported(state, "post-process anti-aliasing");
  }

  TEST(RasterBeautyPassState, DefaultsImportedOpenGLMSAAToPerFragmentShading) {
    QJsonObject execution;
    execution["backend"] = "opengl";
    QJsonObject sampling;
    sampling["msaaSamples"] = 4;
    QJsonObject json;
    json["execution"] = execution;
    json["sampling"] = sampling;

    const RasterBeautyPassState state = RasterBeautyPassState::fromJson(json);

    EXPECT_EQ(Rasterizer::MSAAShadingMode::PerFragment, state.sampling().msaaShadingMode());
    EXPECT_EQ("per_fragment", state.toJson()
                                .value("sampling")
                                .toObject()
                                .value("msaaShadingMode")
                                .toString()
                                .toStdString());
  }

  TEST(RasterBeautyPassState, RejectsUnsupportedOpenGLFramebufferState) {
    RasterBeautyPassState colorLoad;
    colorLoad.framebuffer().setColorLoadOp(Rasterizer::AttachmentLoadOp::Load);
    expectOpenGLUnsupported(colorLoad, "color attachment load");

    RasterBeautyPassState depthLoad;
    depthLoad.framebuffer().setDepthLoadOp(Rasterizer::AttachmentLoadOp::Load);
    expectOpenGLUnsupported(depthLoad, "depth attachment load");

    RasterBeautyPassState stencilLoad;
    stencilLoad.framebuffer().setStencilLoadOp(Rasterizer::AttachmentLoadOp::Load);
    expectOpenGLUnsupported(stencilLoad, "stencil attachment load");
  }

  TEST(RasterBeautyPassState, AppliesShadowStateToOpenGLRasterizer) {
    RasterBeautyPassState state;
    state.shadows().setShadowMapsEnabled(true);
    engine::raster::OpenGLRasterizer rasterizer(nullptr);

    EXPECT_NO_THROW(state.applyTo(rasterizer));
    EXPECT_TRUE(rasterizer.shadowMapsEnabled());
  }

  TEST(RasterBeautyPassState, AppliesImportedStateToRasterizer) {
    QJsonObject execution;
    execution["queueSize"] = 7;
    execution["backend"] = "opengl";
    QJsonObject geometry;
    geometry["lod"] = 3;
    geometry["cullMode"] = "back";
    QJsonObject sampling;
    sampling["msaaSamples"] = 4;
    sampling["msaaShadingMode"] = "per_fragment";
    sampling["postProcessAA"] = "smaa";
    QJsonObject framebuffer;
    framebuffer["colorLoadOp"] = "load";
    framebuffer["colorStoreOp"] = "discard";
    framebuffer["colorWriteMask"] = "g";
    framebuffer["blending"] = true;
    framebuffer["blendSource"] = "constant_alpha";
    framebuffer["blendDestination"] = "one_minus_constant_alpha";
    framebuffer["blendOp"] = "max";
    framebuffer["blendConstantAlpha"] = 0.25;
    framebuffer["alphaTest"] = true;
    framebuffer["alphaFunc"] = "greater";
    framebuffer["alphaReference"] = 0.6;
    framebuffer["depthFunc"] = "greater_equal";
    framebuffer["depthClearValue"] = 12.5;
    framebuffer["depthLoadOp"] = "load";
    framebuffer["depthStoreOp"] = "discard";
    framebuffer["depthWrite"] = false;
    framebuffer["stencilTest"] = true;
    framebuffer["stencilFunc"] = "equal";
    framebuffer["stencilReference"] = 12;
    framebuffer["stencilMask"] = 15;
    framebuffer["stencilClearValue"] = 3;
    framebuffer["stencilLoadOp"] = "clear";
    framebuffer["stencilStoreOp"] = "discard";
    framebuffer["stencilWriteMask"] = 240;
    framebuffer["stencilFailOp"] = "zero";
    framebuffer["stencilDepthFailOp"] = "increment_clamp";
    framebuffer["stencilPassOp"] = "invert";
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
    const RasterBeautyPassState state = RasterBeautyPassState::fromJson(json);
    state.applyTo(rasterizer);

    EXPECT_EQ(3, rasterizer.lod());
    EXPECT_TRUE(state.execution().backend().isOpenGL());
    EXPECT_EQ(7, rasterizer.queueSize());
    EXPECT_EQ(Rasterizer::CullMode::Back, rasterizer.cullMode());
    EXPECT_TRUE(rasterizer.hasCullModeOverride());
    EXPECT_EQ(4, rasterizer.msaaSamples());
    EXPECT_EQ(Rasterizer::MSAAShadingMode::PerFragment, rasterizer.msaaShadingMode());
    EXPECT_EQ(Rasterizer::PostProcessAA::SMAA, rasterizer.postProcessAA());
    EXPECT_EQ(Rasterizer::AttachmentLoadOp::Load, rasterizer.colorLoadOp());
    EXPECT_EQ(Rasterizer::AttachmentStoreOp::Discard, rasterizer.colorStoreOp());
    EXPECT_EQ(Rasterizer::ColorWriteGreen, rasterizer.colorWriteMask());
    EXPECT_TRUE(rasterizer.blendingEnabled());
    EXPECT_EQ(Rasterizer::BlendFactor::ConstantAlpha, rasterizer.sourceBlendFactor());
    EXPECT_EQ(Rasterizer::BlendFactor::OneMinusConstantAlpha, rasterizer.destinationBlendFactor());
    EXPECT_EQ(Rasterizer::BlendOp::Max, rasterizer.blendOp());
    EXPECT_EQ(0.25, rasterizer.blendConstantAlpha());
    EXPECT_TRUE(rasterizer.alphaTestEnabled());
    EXPECT_EQ(Rasterizer::AlphaFunc::Greater, rasterizer.alphaFunc());
    EXPECT_EQ(0.6, rasterizer.alphaReference());
    EXPECT_EQ(Rasterizer::DepthFunc::GreaterEqual, rasterizer.depthFunc());
    EXPECT_EQ(12.5, rasterizer.depthClearValue());
    EXPECT_EQ(Rasterizer::AttachmentLoadOp::Load, rasterizer.depthLoadOp());
    EXPECT_EQ(Rasterizer::AttachmentStoreOp::Discard, rasterizer.depthStoreOp());
    EXPECT_FALSE(rasterizer.depthWriteEnabled());
    EXPECT_TRUE(rasterizer.stencilTestEnabled());
    EXPECT_EQ(Rasterizer::StencilFunc::Equal, rasterizer.stencilFunc());
    EXPECT_EQ(12, rasterizer.stencilReference());
    EXPECT_EQ(15, rasterizer.stencilMask());
    EXPECT_EQ(3, rasterizer.stencilClearValue());
    EXPECT_EQ(Rasterizer::AttachmentLoadOp::Clear, rasterizer.stencilLoadOp());
    EXPECT_EQ(Rasterizer::AttachmentStoreOp::Discard, rasterizer.stencilStoreOp());
    EXPECT_EQ(240, rasterizer.stencilWriteMask());
    EXPECT_EQ(Rasterizer::StencilOp::Zero, rasterizer.stencilFailOp());
    EXPECT_EQ(Rasterizer::StencilOp::IncrementClamp, rasterizer.stencilDepthFailOp());
    EXPECT_EQ(Rasterizer::StencilOp::Invert, rasterizer.stencilPassOp());
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

  TEST(RasterBeautyPassState, WritesOnlyToRasterAOVPasses) {
    RenderPlan plan;
    RenderPassNode rasterAOV;
    rasterAOV.id = "depth_aov";
    rasterAOV.kind = RenderPassKind::AOV;
    rasterAOV.executor = RenderExecutorKind::Rasterizer;
    plan.addPass(rasterAOV);

    RenderPassNode raytraceAOV;
    raytraceAOV.id = "normal_aov";
    raytraceAOV.kind = RenderPassKind::AOV;
    raytraceAOV.executor = RenderExecutorKind::Raytracer;
    plan.addPass(raytraceAOV);

    RenderPassNode rasterBeauty;
    rasterBeauty.id = "raster_beauty";
    rasterBeauty.kind = RenderPassKind::Beauty;
    rasterBeauty.executor = RenderExecutorKind::Rasterizer;
    plan.addPass(rasterBeauty);

    RasterBeautyPassState state;
    state.sampling().setMSAASamples(4);

    EXPECT_EQ(1u, state.writeToRasterAOVPasses(plan));
    ASSERT_NE(nullptr, plan.passes()[0].state);
    EXPECT_EQ(4, RasterBeautyPassState::fromPass(plan.passes()[0])->sampling().msaaSamples());
    EXPECT_EQ(nullptr, plan.passes()[1].state);
    EXPECT_EQ(nullptr, plan.passes()[2].state);
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
