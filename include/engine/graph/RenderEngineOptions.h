#pragma once

#include "core/Color.h"
#include "core/math/Rect.h"

#include <QJsonObject>

#include <cstdint>
#include <optional>
#include <string>

namespace engine::graph {
  class RasterBeautyPassState;
  class RasterShadowPassState;
  class RaytracerBeautyPassState;
  class WireframePassState;
  enum class RenderPostProcessAA;

  /**
    * Intent-level advanced controls for raytracer-backed views.
    *
    * Fields are optional so subviews can inherit global options and override
    * only the controls they need.
    */
  class RenderRaytracerOptions {
  public:
    bool empty() const;
    QJsonObject toJson() const;
    static RenderRaytracerOptions fromJson(const QJsonObject& object,
                                           const std::string& path = "raytracer");

    RenderRaytracerOptions mergedWith(const RenderRaytracerOptions& overrides) const;
    RaytracerBeautyPassState beautyPassState() const;

    void setMaximumRecursionDepth(int depth);
    void setMaximumThreads(int threads);
    void setQueueSize(int queueSize);
    void setSampler(std::string sampler);
    void setSamplesPerPixel(int samples);
    void setViewPlane(std::string viewPlane);

    std::optional<int> maximumRecursionDepth() const;
    std::optional<int> maximumThreads() const;
    std::optional<int> queueSize() const;
    std::optional<std::string> sampler() const;
    std::optional<int> samplesPerPixel() const;
    std::optional<std::string> viewPlane() const;

  private:
    std::optional<int> m_maximumRecursionDepth;
    std::optional<int> m_maximumThreads;
    std::optional<int> m_queueSize;
    std::optional<std::string> m_sampler;
    std::optional<int> m_samplesPerPixel;
    std::optional<std::string> m_viewPlane;
  };

  /**
    * Intent-level advanced controls for rasterizer-backed views.
    */
  class RenderRasterizerOptions {
  public:
    bool empty() const;
    QJsonObject toJson() const;
    static RenderRasterizerOptions fromJson(const QJsonObject& object,
                                            const std::string& path = "rasterizer");

    RenderRasterizerOptions mergedWith(const RenderRasterizerOptions& overrides) const;
    RasterBeautyPassState beautyPassState(int targetSampleCount, RenderPostProcessAA aa,
                                          bool includeImagePostProcessAA,
                                          bool includeShadowMapEnable) const;
    RasterShadowPassState shadowPassState() const;

    void setMaximumThreads(int threads);
    void setQueueSize(int queueSize);
    void setLod(int lod);
    void setCullMode(std::string mode);
    void setMSAASamples(int samples);
    void setMSAAShadingMode(std::string mode);
    void setViewportRect(const Recti& rect);
    void setScissorRect(const Recti& rect);
    void setDepthBias(double bias);
    void setColorWriteMask(std::uint8_t mask);
    void setBlendingEnabled(bool enabled);
    void setBlendFactors(std::string source, std::string destination);
    void setBlendOp(std::string op);
    void setBlendConstant(const Colord& color, double alpha);
    void setAlphaTestEnabled(bool enabled);
    void setAlphaFunc(std::string func, double reference);
    void setShadowMapSize(int size);
    void setShadowCascadeCount(int count);
    void setShadowCascadeSplitLambda(double lambda);
    void setShadowBias(double bias);
    void setShadowSlopeBias(double bias);
    void setShadowFilterRadius(int radius);
    void setShadowFilterMode(std::string mode);

    std::optional<int> maximumThreads() const;
    std::optional<int> queueSize() const;
    std::optional<int> lod() const;
    std::optional<std::string> cullMode() const;
    std::optional<int> msaaSamples() const;
    std::optional<std::string> msaaShadingMode() const;
    std::optional<int> shadowMapSize() const;
    std::optional<int> shadowCascadeCount() const;
    std::optional<double> shadowBias() const;
    std::optional<int> shadowFilterRadius() const;
    std::optional<std::string> shadowFilterMode() const;

  private:
    std::optional<int> m_maximumThreads;
    std::optional<int> m_queueSize;
    std::optional<int> m_lod;
    std::optional<std::string> m_cullMode;
    std::optional<int> m_msaaSamples;
    std::optional<std::string> m_msaaShadingMode;
    std::optional<Recti> m_viewportRect;
    std::optional<Recti> m_scissorRect;
    std::optional<double> m_depthBias;
    std::optional<std::uint8_t> m_colorWriteMask;
    std::optional<bool> m_blendingEnabled;
    std::optional<std::string> m_sourceBlendFactor;
    std::optional<std::string> m_destinationBlendFactor;
    std::optional<std::string> m_blendOp;
    std::optional<Colord> m_blendConstantColor;
    std::optional<double> m_blendConstantAlpha;
    std::optional<bool> m_alphaTestEnabled;
    std::optional<std::string> m_alphaFunc;
    std::optional<double> m_alphaReference;
    std::optional<int> m_shadowMapSize;
    std::optional<int> m_shadowCascadeCount;
    std::optional<double> m_shadowCascadeSplitLambda;
    std::optional<double> m_shadowBias;
    std::optional<double> m_shadowSlopeBias;
    std::optional<int> m_shadowFilterRadius;
    std::optional<std::string> m_shadowFilterMode;
  };

  /**
    * Intent-level advanced controls for wireframe-backed views.
    */
  class RenderWireframeOptions {
  public:
    bool empty() const;
    QJsonObject toJson() const;
    static RenderWireframeOptions fromJson(const QJsonObject& object,
                                           const std::string& path = "wireframe");

    RenderWireframeOptions mergedWith(const RenderWireframeOptions& overrides) const;
    WireframePassState passState() const;

    void setLod(int lod);
    std::optional<int> lod() const;

  private:
    std::optional<int> m_lod;
  };

  /**
    * Engine-specific advanced controls attached to render intent.
    */
  class RenderEngineOptions {
  public:
    bool empty() const;
    QJsonObject toJson() const;
    static RenderEngineOptions fromJson(const QJsonObject& object,
                                        const std::string& path = "engineOptions");

    RenderEngineOptions mergedWith(const RenderEngineOptions& overrides) const;

    RenderRaytracerOptions& raytracer();
    RenderRasterizerOptions& rasterizer();
    RenderWireframeOptions& wireframe();
    const RenderRaytracerOptions& raytracer() const;
    const RenderRasterizerOptions& rasterizer() const;
    const RenderWireframeOptions& wireframe() const;

  private:
    RenderRaytracerOptions m_raytracer;
    RenderRasterizerOptions m_rasterizer;
    RenderWireframeOptions m_wireframe;
  };
}
