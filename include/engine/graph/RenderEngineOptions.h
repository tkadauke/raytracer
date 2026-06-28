#pragma once

#include "core/Color.h"
#include "core/math/Rect.h"
#include "engine/raster/RasterBackend.h"
#include "engine/graph/TracingExecutionPreference.h"
#include "render/WavefrontIntersectionBackend.h"

#include <QJsonObject>

#include <cstdint>
#include <optional>
#include <string>

namespace engine::graph {
  class RasterBeautyPassState;
  class RasterShadowPassState;
  class RasterVisibilityPassState;
  class RaytracerBeautyPassState;
  class WireframePassState;
  enum class RenderPostProcessAA;

  /**
    * Intent-level request for a graph-visible raster visibility preprocessing
    * pass. `Auto` is reserved for compiler heuristics; today it requests the
    * same baseline pass as `On`.
    */
  enum class RenderVisibilityCulling { Off, On, Auto };

  /**
    * Intent-level shadow implementation for rasterizer-backed preview graphs.
    *
    * `ShadowMaps` preserves the existing raster shadow-map path. `RayTraced`
    * asks the graph to build a per-pixel shadow mask with the intersection
    * service and composite it over the raster beauty result.
    */
  enum class RenderRasterShadowMode { ShadowMaps, RayTraced };

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
    void setIntegrator(std::string integrator);
    void setTracingBackend(std::string backend);
    void setTracingBackend(render::WavefrontIntersectionBackendChoice backend);
    void setTracingExecution(TracingExecutionPreference preference);
    void setTracingExecution(std::string preference);
    void setIntersectionBackend(std::string backend);
    void setIntersectionBackend(render::WavefrontIntersectionBackendChoice backend);
    void setRussianRouletteDepth(int depth);
    void setDirectLightSamples(int samples);
    void setGpuPrimarySampleChunkSize(int samples);
    void setSampler(std::string sampler);
    void clearSampler();
    void setSamplesPerPixel(int samples);
    void setSamplingSeed(std::uint64_t seed);
    void setSampleStreamMode(std::string mode);
    void setViewPlane(std::string viewPlane);
    void setConvergenceEnabled(bool enabled);
    void setConvergenceActiveSampleFractionThreshold(double fraction);
    void setConvergenceRadianceDeltaRmsThreshold(double threshold);
    void setAdaptiveSamplingEnabled(bool enabled);
    void setAdaptiveMinimumSamples(int samples);
    void setAdaptiveStddevThreshold(double threshold);
    void setDenoiser(std::string denoiser);
    void setDenoiseRadius(int radius);
    void setDenoiseColorSigma(double sigma);

    std::optional<int> maximumRecursionDepth() const;
    std::optional<int> maximumThreads() const;
    std::optional<int> queueSize() const;
    std::optional<std::string> integrator() const;
    std::optional<render::WavefrontIntersectionBackendChoice> tracingBackend() const;
    std::optional<TracingExecutionPreference> tracingExecution() const;
    std::optional<render::WavefrontIntersectionBackendChoice> intersectionBackend() const;
    std::optional<int> russianRouletteDepth() const;
    std::optional<int> directLightSamples() const;
    std::optional<int> gpuPrimarySampleChunkSize() const;
    std::optional<std::string> sampler() const;
    std::optional<int> samplesPerPixel() const;
    std::optional<std::uint64_t> samplingSeed() const;
    std::optional<std::string> sampleStreamMode() const;
    std::optional<std::string> viewPlane() const;
    std::optional<bool> convergenceEnabled() const;
    std::optional<double> convergenceActiveSampleFractionThreshold() const;
    std::optional<double> convergenceRadianceDeltaRmsThreshold() const;
    std::optional<bool> adaptiveSamplingEnabled() const;
    std::optional<int> adaptiveMinimumSamples() const;
    std::optional<double> adaptiveStddevThreshold() const;
    std::optional<std::string> denoiser() const;
    std::optional<int> denoiseRadius() const;
    std::optional<double> denoiseColorSigma() const;

  private:
    std::optional<int> m_maximumRecursionDepth;
    std::optional<int> m_maximumThreads;
    std::optional<int> m_queueSize;
    std::optional<std::string> m_integrator;
    std::optional<render::WavefrontIntersectionBackendChoice> m_tracingBackend;
    std::optional<TracingExecutionPreference> m_tracingExecution;
    std::optional<render::WavefrontIntersectionBackendChoice> m_intersectionBackend;
    std::optional<int> m_russianRouletteDepth;
    std::optional<int> m_directLightSamples;
    std::optional<int> m_gpuPrimarySampleChunkSize;
    std::optional<std::string> m_sampler;
    std::optional<int> m_samplesPerPixel;
    std::optional<std::uint64_t> m_samplingSeed;
    std::optional<std::string> m_sampleStreamMode;
    std::optional<std::string> m_viewPlane;
    std::optional<bool> m_convergenceEnabled;
    std::optional<double> m_convergenceActiveSampleFractionThreshold;
    std::optional<double> m_convergenceRadianceDeltaRmsThreshold;
    std::optional<bool> m_adaptiveSamplingEnabled;
    std::optional<int> m_adaptiveMinimumSamples;
    std::optional<double> m_adaptiveStddevThreshold;
    std::optional<std::string> m_denoiser;
    std::optional<int> m_denoiseRadius;
    std::optional<double> m_denoiseColorSigma;
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
    RasterVisibilityPassState visibilityPassState() const;

    void setMaximumThreads(int threads);
    void setQueueSize(int queueSize);
    void setBackend(engine::raster::RasterBackend backend);
    void setBackend(std::string backend);
    void setLod(int lod);
    void setTessellationQuality(std::string quality);
    void setMaximumScreenSpaceError(double pixels);
    void setCullMode(std::string mode);
    void setVisibilityCulling(RenderVisibilityCulling mode);
    void setVisibilityCulling(std::string mode);
    void setDepthPrepass(std::string mode);
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
    void setShadowMode(RenderRasterShadowMode mode);
    void setShadowMode(std::string mode);

    std::optional<int> maximumThreads() const;
    std::optional<int> queueSize() const;
    std::optional<engine::raster::RasterBackend> backend() const;
    std::optional<int> lod() const;
    std::optional<std::string> tessellationQuality() const;
    std::optional<double> maximumScreenSpaceError() const;
    std::optional<std::string> cullMode() const;
    std::optional<RenderVisibilityCulling> visibilityCulling() const;
    std::optional<std::string> depthPrepass() const;
    std::optional<int> msaaSamples() const;
    std::optional<std::string> msaaShadingMode() const;
    std::optional<int> shadowMapSize() const;
    std::optional<int> shadowCascadeCount() const;
    std::optional<double> shadowCascadeSplitLambda() const;
    std::optional<double> shadowBias() const;
    std::optional<double> shadowSlopeBias() const;
    std::optional<int> shadowFilterRadius() const;
    std::optional<std::string> shadowFilterMode() const;
    std::optional<RenderRasterShadowMode> shadowMode() const;

  private:
    std::optional<int> m_maximumThreads;
    std::optional<int> m_queueSize;
    std::optional<engine::raster::RasterBackend> m_backend;
    std::optional<int> m_lod;
    std::optional<std::string> m_tessellationQuality;
    std::optional<double> m_maximumScreenSpaceError;
    std::optional<std::string> m_cullMode;
    std::optional<RenderVisibilityCulling> m_visibilityCulling;
    std::optional<std::string> m_depthPrepass;
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
    std::optional<RenderRasterShadowMode> m_shadowMode;

    static RenderVisibilityCulling visibilityCullingFromString(const std::string& value,
                                                               const std::string& path);
    static const char* visibilityCullingName(RenderVisibilityCulling mode);
    static RenderRasterShadowMode shadowModeFromString(const std::string& value,
                                                       const std::string& path);
    static const char* shadowModeName(RenderRasterShadowMode mode);
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
