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
#include <memory>
#include <optional>
#include <string>

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

    void setLod(int lod);
    void setCullMode(Rasterizer::CullMode mode);

  private:
    int m_lod{0};
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

    void setMSAASamples(int samples);
    void setMSAAShadingMode(Rasterizer::MSAAShadingMode mode);
    void setPostProcessAA(Rasterizer::PostProcessAA aa);

    int msaaSamples() const;

  private:
    int m_msaaSamples{1};
    Rasterizer::MSAAShadingMode m_msaaShadingMode{Rasterizer::MSAAShadingMode::PerSample};
    Rasterizer::PostProcessAA m_postProcessAA{Rasterizer::PostProcessAA::None};
  };

  /**
    * Framebuffer-space, depth-bias, alpha, and color-output controls.
    */
  class RasterFramebufferState {
  public:
    using Rasterizer = engine::raster::Rasterizer;

    static RasterFramebufferState fromJson(const QJsonObject& object,
                                           const std::string& path = "parameters.framebuffer");
    QJsonObject toJson() const;
    bool empty() const;
    void applyTo(Rasterizer& rasterizer) const;

    void setViewportRect(const Recti& rect);
    void setScissorRect(const Recti& rect);
    void setDepthBias(double bias);
    void setColorWriteMask(std::uint8_t mask);
    void setBlendingEnabled(bool enabled);
    void setBlendFactors(Rasterizer::BlendFactor source, Rasterizer::BlendFactor destination);
    void setBlendOp(Rasterizer::BlendOp op);
    void setBlendConstant(const Colord& color, double alpha);
    void setAlphaTestEnabled(bool enabled);
    void setAlphaFunc(Rasterizer::AlphaFunc func, double reference);

  private:
    std::optional<Recti> m_viewportRect;
    std::optional<Recti> m_scissorRect;
    double m_depthBias{0.0};
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

    void writeTo(RenderPassNode& pass) const;
    std::size_t writeToRasterShadowPasses(RenderPlan& plan) const;

    RasterShadowState& shadows();
    const RasterShadowState& shadows() const;

  private:
    RasterShadowState m_shadows;
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

    void writeTo(RenderPassNode& pass) const;
    std::size_t writeToRasterBeautyPasses(RenderPlan& plan) const;
    std::size_t writeToRasterAOVPasses(RenderPlan& plan) const;

    RasterExecutionState& execution();
    RasterGeometryState& geometry();
    RasterSamplingState& sampling();
    RasterFramebufferState& framebuffer();
    RasterShadowState& shadows();

    const RasterExecutionState& execution() const;
    const RasterGeometryState& geometry() const;
    const RasterSamplingState& sampling() const;
    const RasterFramebufferState& framebuffer() const;
    const RasterShadowState& shadows() const;

  private:
    RasterExecutionState m_execution;
    RasterGeometryState m_geometry;
    RasterSamplingState m_sampling;
    RasterFramebufferState m_framebuffer;
    RasterShadowState m_shadows;
  };

}
