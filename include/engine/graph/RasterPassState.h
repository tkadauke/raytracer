#pragma once

#include "core/Color.h"
#include "core/math/Rect.h"
#include "engine/graph/RenderGraphTypes.h"
#include "engine/graph/RenderPassState.h"
#include "engine/raster/RasterBackend.h"
#include "engine/raster/Rasterizer.h"

#include <QJsonObject>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>

namespace engine::raster {
  class OpenGLRasterizer;
}

namespace engine::graph {
  class RenderPlan;

  /**
    * Performance and scheduling hints for a raster pass.
    *
    * These settings affect how the pass runs, not the authored scene intent.
    */
  class RasterExecutionState {
  public:
    using Rasterizer = engine::raster::Rasterizer;

    static RasterExecutionState fromJson(const QJsonObject& object,
                                         const std::string& path = "parameters.execution");
    QJsonObject toJson() const;
    bool empty() const;
    void applyTo(Rasterizer& rasterizer) const;

    void setMaximumThreads(int threads);
    void setQueueSize(int queueSize);
    void setBackend(engine::raster::RasterBackend backend);

    engine::raster::RasterBackend backend() const;

  private:
    std::optional<int> m_maximumThreads;
    std::optional<int> m_queueSize;
    std::optional<engine::raster::RasterBackend> m_backend;
  };

  /**
    * Scene-to-triangle controls for a raster pass.
    */
  class RasterGeometryState {
  public:
    using Rasterizer = engine::raster::Rasterizer;

    static RasterGeometryState fromJson(const QJsonObject& object,
                                        const std::string& path = "parameters.geometry");
    QJsonObject toJson() const;
    bool empty() const;
    void applyTo(Rasterizer& rasterizer) const;
    void applyTo(engine::raster::OpenGLRasterizer& rasterizer) const;

    void setLod(int lod);
    void setTessellationQuality(Rasterizer::TessellationQuality quality);
    void setMaximumScreenSpaceError(double pixels);
    void clearMaximumScreenSpaceErrorOverride();
    void setCullMode(Rasterizer::CullMode mode);

    int lod() const;
    Rasterizer::TessellationQuality tessellationQuality() const;
    double maximumScreenSpaceError() const;
    bool hasMaximumScreenSpaceErrorOverride() const;
    std::optional<Rasterizer::CullMode> cullModeOverride() const;
    Rasterizer::CullMode cullMode() const;
    bool hasCullModeOverride() const;

  private:
    int m_lod{0};
    Rasterizer::TessellationQuality m_tessellationQuality{
      Rasterizer::TessellationQuality::Balanced};
    std::optional<double> m_maximumScreenSpaceError;
    std::optional<Rasterizer::CullMode> m_cullMode;
  };

  /**
    * Anti-aliasing controls for a raster pass.
    */
  class RasterSamplingState {
  public:
    using Rasterizer = engine::raster::Rasterizer;

    static RasterSamplingState fromJson(const QJsonObject& object,
                                        const std::string& path = "parameters.sampling");
    QJsonObject toJson() const;
    bool empty() const;
    void applyTo(Rasterizer& rasterizer) const;
    void applyTo(engine::raster::OpenGLRasterizer& rasterizer) const;
    void validateSupportedByOpenGL() const;

    void setMSAASamples(int samples);
    void setMSAAShadingMode(Rasterizer::MSAAShadingMode mode);
    void setPostProcessAA(Rasterizer::PostProcessAA aa);

    int msaaSamples() const;
    Rasterizer::MSAAShadingMode msaaShadingMode() const;

  private:
    int m_msaaSamples{1};
    Rasterizer::MSAAShadingMode m_msaaShadingMode{Rasterizer::MSAAShadingMode::PerSample};
    Rasterizer::PostProcessAA m_postProcessAA{Rasterizer::PostProcessAA::None};
  };

  /**
    * Optional depth prepass controls for measured opaque raster passes.
    */
  class RasterDepthPrepassState {
  public:
    using Rasterizer = engine::raster::Rasterizer;

    static RasterDepthPrepassState fromJson(const QJsonObject& object,
                                            const std::string& path = "parameters.depthPrepass");
    QJsonObject toJson() const;
    bool empty() const;
    void applyTo(Rasterizer& rasterizer) const;
    void applyTo(engine::raster::OpenGLRasterizer& rasterizer) const;

    void setMode(Rasterizer::DepthPrepassMode mode);
    Rasterizer::DepthPrepassMode mode() const;

  private:
    Rasterizer::DepthPrepassMode m_mode{Rasterizer::DepthPrepassMode::Off};
  };

  /**
    * Framebuffer-space, depth-bias, stencil, alpha, and color-output controls.
    */
  class RasterFramebufferState {
  public:
    using Rasterizer = engine::raster::Rasterizer;

    static RasterFramebufferState fromJson(const QJsonObject& object,
                                           const std::string& path = "parameters.framebuffer");
    QJsonObject toJson() const;
    bool empty() const;
    void applyTo(Rasterizer& rasterizer) const;
    void applyTo(engine::raster::OpenGLRasterizer& rasterizer) const;
    void validateSupportedByOpenGL() const;

    void setViewportRect(const Recti& rect);
    void setScissorRect(const Recti& rect);
    void setColorLoadOp(Rasterizer::AttachmentLoadOp op);
    void setColorStoreOp(Rasterizer::AttachmentStoreOp op);
    void setDepthFunc(Rasterizer::DepthFunc func);
    void setDepthBias(double bias);
    void setDepthClearValue(double value);
    void setDepthLoadOp(Rasterizer::AttachmentLoadOp op);
    void setDepthStoreOp(Rasterizer::AttachmentStoreOp op);
    void setDepthWriteEnabled(bool enabled);
    void setColorWriteMask(std::uint8_t mask);
    void setBlendingEnabled(bool enabled);
    void setBlendFactors(Rasterizer::BlendFactor source, Rasterizer::BlendFactor destination);
    void setBlendOp(Rasterizer::BlendOp op);
    void setBlendConstant(const Colord& color, double alpha);
    void setAlphaTestEnabled(bool enabled);
    void setAlphaFunc(Rasterizer::AlphaFunc func, double reference);
    void setStencilTestEnabled(bool enabled);
    void setStencilFunc(Rasterizer::StencilFunc func, std::uint8_t reference,
                        std::uint8_t mask = 0xff);
    void setStencilClearValue(std::uint8_t value);
    void setStencilLoadOp(Rasterizer::AttachmentLoadOp op);
    void setStencilStoreOp(Rasterizer::AttachmentStoreOp op);
    void setStencilWriteMask(std::uint8_t mask);
    void setStencilOps(Rasterizer::StencilOp stencilFail, Rasterizer::StencilOp depthFail,
                       Rasterizer::StencilOp pass);
    void configureStencilWritePass(std::uint8_t value);

    bool supportsFrontToBackVisibilityOrdering() const;
    bool stencilTestEnabled() const;

  private:
    std::optional<Recti> m_viewportRect;
    std::optional<Recti> m_scissorRect;
    Rasterizer::AttachmentLoadOp m_colorLoadOp{Rasterizer::AttachmentLoadOp::Clear};
    Rasterizer::AttachmentStoreOp m_colorStoreOp{Rasterizer::AttachmentStoreOp::Store};
    Rasterizer::DepthFunc m_depthFunc{Rasterizer::DepthFunc::Less};
    double m_depthBias{0.0};
    double m_depthClearValue{std::numeric_limits<double>::infinity()};
    Rasterizer::AttachmentLoadOp m_depthLoadOp{Rasterizer::AttachmentLoadOp::Clear};
    Rasterizer::AttachmentStoreOp m_depthStoreOp{Rasterizer::AttachmentStoreOp::Store};
    bool m_depthWriteEnabled{true};
    std::uint8_t m_colorWriteMask{Rasterizer::ColorWriteAll};
    bool m_blendingEnabled{false};
    Rasterizer::BlendFactor m_sourceBlendFactor{Rasterizer::BlendFactor::One};
    Rasterizer::BlendFactor m_destinationBlendFactor{Rasterizer::BlendFactor::Zero};
    Rasterizer::BlendOp m_blendOp{Rasterizer::BlendOp::Add};
    Colord m_blendConstantColor{Colord::white()};
    double m_blendConstantAlpha{1.0};
    bool m_alphaTestEnabled{false};
    Rasterizer::AlphaFunc m_alphaFunc{Rasterizer::AlphaFunc::Always};
    double m_alphaReference{0.0};
    bool m_stencilTestEnabled{false};
    Rasterizer::StencilFunc m_stencilFunc{Rasterizer::StencilFunc::Always};
    std::uint8_t m_stencilReference{0};
    std::uint8_t m_stencilMask{0xff};
    std::uint8_t m_stencilClearValue{0};
    Rasterizer::AttachmentLoadOp m_stencilLoadOp{Rasterizer::AttachmentLoadOp::Clear};
    Rasterizer::AttachmentStoreOp m_stencilStoreOp{Rasterizer::AttachmentStoreOp::Store};
    std::uint8_t m_stencilWriteMask{0xff};
    Rasterizer::StencilOp m_stencilFailOp{Rasterizer::StencilOp::Keep};
    Rasterizer::StencilOp m_stencilDepthFailOp{Rasterizer::StencilOp::Keep};
    Rasterizer::StencilOp m_stencilPassOp{Rasterizer::StencilOp::Keep};
  };

  /**
    * Directional shadow-map controls for a raster pass.
    */
  class RasterShadowState {
  public:
    using Rasterizer = engine::raster::Rasterizer;

    static RasterShadowState fromJson(const QJsonObject& object,
                                      const std::string& path = "parameters.shadows");
    QJsonObject toJson() const;
    bool empty() const;
    void applyTo(Rasterizer& rasterizer) const;
    void applyTo(engine::raster::OpenGLRasterizer& rasterizer) const;

    void setShadowMapsEnabled(bool enabled);
    void setShadowMapSize(int size);
    void setShadowCascadeCount(int count);
    void setShadowCascadeSplitLambda(double lambda);
    void setShadowBias(double bias);
    void setShadowSlopeBias(double bias);
    void setShadowFilterRadius(int radius);
    void setShadowFilterMode(Rasterizer::ShadowFilterMode mode);

    bool enabled() const;
    int mapSize() const;
    RenderResourceDescriptor resourceDescriptor(RenderResourceId id, std::string name) const;

  private:
    bool m_enabled{false};
    int m_mapSize{256};
    int m_cascadeCount{1};
    double m_cascadeSplitLambda{0.5};
    double m_bias{1e-3};
    double m_slopeBias{0.0};
    int m_filterRadius{0};
    Rasterizer::ShadowFilterMode m_filterMode{Rasterizer::ShadowFilterMode::PCF};
  };

  /**
    * Typed state for graph shadow-map request passes.
    *
    * The first CPU graph slice still lets the rasterizer build directional
    * shadow maps internally, but this pass state makes the request and preview
    * policy visible on the shadow node instead of hiding it in the beauty
    * payload.
    */
  class RasterShadowPassState : public RenderPassState {
  public:
    using Rasterizer = engine::raster::Rasterizer;

    static RasterShadowPassState fromJson(const QJsonObject& object,
                                          const std::string& path = "parameters");
    static RasterShadowPassState previewDefaults();
    static const RasterShadowPassState* fromPass(const RenderPassNode& pass);
    static RasterShadowPassState valueFromPass(const RenderPassNode& pass);

    const RasterShadowPassState* asRasterShadowPassState() const override;
    QJsonObject toJson() const override;
    bool empty() const;
    void applyTo(Rasterizer& rasterizer) const;
    void applyTo(engine::raster::OpenGLRasterizer& rasterizer) const;

    void writeTo(RenderPassNode& pass) const;
    std::size_t writeToRasterShadowPasses(RenderPlan& plan) const;

    RasterShadowState& shadows();
    const RasterShadowState& shadows() const;

  private:
    RasterShadowState m_shadows;
  };

  /**
    * Typed state for graph visibility preprocessing passes.
    *
    * Visibility preprocessing must use the same scene-to-triangle controls as
    * the raster pass that consumes its result. The first implementation only
    * records baseline metrics, but those metrics still need to reflect the
    * compiled raster LOD instead of reaching back into mutable scene intent.
    */
  class RasterVisibilityPassState : public RenderPassState {
  public:
    static RasterVisibilityPassState fromJson(const QJsonObject& object,
                                              const std::string& path = "parameters");
    static const RasterVisibilityPassState* fromPass(const RenderPassNode& pass);
    static RasterVisibilityPassState valueFromPass(const RenderPassNode& pass);

    const RasterVisibilityPassState* asRasterVisibilityPassState() const override;
    QJsonObject toJson() const override;
    bool empty() const;

    void writeTo(RenderPassNode& pass) const;

    RasterGeometryState& geometry();
    const RasterGeometryState& geometry() const;
    void setFrontToBackOrderingEnabled(bool enabled);
    bool frontToBackOrderingEnabled() const;

  private:
    RasterGeometryState m_geometry;
    bool m_frontToBackOrderingEnabled{true};
  };

  /**
    * Typed state for the built-in raster beauty graph pass.
    */
  class RasterBeautyPassState : public RenderPassState {
  public:
    using Rasterizer = engine::raster::Rasterizer;

    static RasterBeautyPassState fromJson(const QJsonObject& object,
                                          const std::string& path = "parameters");
    static const RasterBeautyPassState* fromPass(const RenderPassNode& pass);
    static RasterBeautyPassState valueFromPass(const RenderPassNode& pass);

    const RasterBeautyPassState* asRasterBeautyPassState() const override;
    QJsonObject toJson() const override;
    bool empty() const;
    void applyTo(Rasterizer& rasterizer) const;
    void applyTo(engine::raster::OpenGLRasterizer& rasterizer) const;

    void writeTo(RenderPassNode& pass) const;
    std::size_t writeToRasterBeautyPasses(RenderPlan& plan) const;
    std::size_t writeToRasterAOVPasses(RenderPlan& plan) const;

    RasterExecutionState& execution();
    RasterGeometryState& geometry();
    RasterSamplingState& sampling();
    RasterDepthPrepassState& depthPrepass();
    RasterFramebufferState& framebuffer();
    RasterShadowState& shadows();

    const RasterExecutionState& execution() const;
    const RasterGeometryState& geometry() const;
    const RasterSamplingState& sampling() const;
    const RasterDepthPrepassState& depthPrepass() const;
    const RasterFramebufferState& framebuffer() const;
    const RasterShadowState& shadows() const;

  private:
    RasterExecutionState m_execution;
    RasterGeometryState m_geometry;
    RasterSamplingState m_sampling;
    RasterDepthPrepassState m_depthPrepass;
    RasterFramebufferState m_framebuffer;
    RasterShadowState m_shadows;
  };

}
