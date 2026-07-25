#include "engine/graph/RenderEngineOptions.h"

#include "engine/graph/RasterPassState.h"
#include "engine/graph/RaytracerPassState.h"
#include "engine/graph/RenderGraphTypes.h"
#include "engine/graph/WireframePassState.h"
#include "engine/graph/detail/JsonStateHelpers.h"
#include "engine/raster/Rasterizer.h"
#include "core/util/QStringUtil.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <stdexcept>
#include <utility>

namespace engine::graph {
  namespace {
    using Rasterizer = engine::raster::Rasterizer;

    [[noreturn]] void optionsError(const std::string& path, const std::string& message) {
      throw std::runtime_error("Invalid render engine options at " + path + ": " + message);
    }


    std::string colorWriteMaskString(std::uint8_t mask) {
      mask &= Rasterizer::ColorWriteAll;
      if (mask == 0)
        return "none";
      if (mask == Rasterizer::ColorWriteAll)
        return "all";

      std::string result;
      if (mask & Rasterizer::ColorWriteRed)
        result.push_back('r');
      if (mask & Rasterizer::ColorWriteGreen)
        result.push_back('g');
      if (mask & Rasterizer::ColorWriteBlue)
        result.push_back('b');
      return result;
    }

    std::uint8_t colorWriteMaskFromString(const std::string& value, const std::string& path) {
      if (value == "none")
        return 0;
      if (value == "all")
        return Rasterizer::ColorWriteAll;

      std::uint8_t mask = 0;
      for (const char ch : value) {
        if (ch == 'r') {
          mask |= Rasterizer::ColorWriteRed;
        } else if (ch == 'g') {
          mask |= Rasterizer::ColorWriteGreen;
        } else if (ch == 'b') {
          mask |= Rasterizer::ColorWriteBlue;
        } else {
          optionsError(path, "expected r, g, b, all, or none");
        }
      }
      return mask;
    }

    Rasterizer::CullMode cullModeFromString(const std::string& value, const std::string& path) {
      if (value == "both")
        return Rasterizer::CullMode::Both;
      if (value == "back")
        return Rasterizer::CullMode::Back;
      if (value == "front")
        return Rasterizer::CullMode::Front;
      optionsError(path, "expected both, back, or front");
    }

    Rasterizer::TessellationQuality tessellationQualityFromString(const std::string& value,
                                                                  const std::string& path) {
      if (value == "preview")
        return Rasterizer::TessellationQuality::Preview;
      if (value == "balanced")
        return Rasterizer::TessellationQuality::Balanced;
      if (value == "final")
        return Rasterizer::TessellationQuality::Final;
      optionsError(path, "expected preview, balanced, or final");
    }

    Rasterizer::MSAAShadingMode msaaShadingModeFromString(const std::string& value,
                                                          const std::string& path) {
      if (value == "per_sample")
        return Rasterizer::MSAAShadingMode::PerSample;
      if (value == "per_fragment")
        return Rasterizer::MSAAShadingMode::PerFragment;
      optionsError(path, "expected per_sample or per_fragment");
    }

    Rasterizer::PostProcessAA rasterPostProcessAA(RenderPostProcessAA aa) {
      if (aa == RenderPostProcessAA::FXAA)
        return Rasterizer::PostProcessAA::FXAA;
      if (aa == RenderPostProcessAA::SMAA)
        return Rasterizer::PostProcessAA::SMAA;
      if (aa == RenderPostProcessAA::TAA)
        return Rasterizer::PostProcessAA::TAA;
      return Rasterizer::PostProcessAA::None;
    }

    Rasterizer::BlendFactor blendFactorFromString(const std::string& value,
                                                  const std::string& path) {
      if (value == "zero")
        return Rasterizer::BlendFactor::Zero;
      if (value == "one")
        return Rasterizer::BlendFactor::One;
      if (value == "source_color")
        return Rasterizer::BlendFactor::SourceColor;
      if (value == "one_minus_source_color")
        return Rasterizer::BlendFactor::OneMinusSourceColor;
      if (value == "source_alpha")
        return Rasterizer::BlendFactor::SourceAlpha;
      if (value == "one_minus_source_alpha")
        return Rasterizer::BlendFactor::OneMinusSourceAlpha;
      if (value == "destination_color")
        return Rasterizer::BlendFactor::DestinationColor;
      if (value == "one_minus_destination_color")
        return Rasterizer::BlendFactor::OneMinusDestinationColor;
      if (value == "constant_color")
        return Rasterizer::BlendFactor::ConstantColor;
      if (value == "one_minus_constant_color")
        return Rasterizer::BlendFactor::OneMinusConstantColor;
      if (value == "constant_alpha")
        return Rasterizer::BlendFactor::ConstantAlpha;
      if (value == "one_minus_constant_alpha")
        return Rasterizer::BlendFactor::OneMinusConstantAlpha;
      optionsError(path, "unknown blend factor");
    }

    Rasterizer::BlendOp blendOpFromString(const std::string& value, const std::string& path) {
      if (value == "add")
        return Rasterizer::BlendOp::Add;
      if (value == "subtract")
        return Rasterizer::BlendOp::Subtract;
      if (value == "reverse_subtract")
        return Rasterizer::BlendOp::ReverseSubtract;
      if (value == "min")
        return Rasterizer::BlendOp::Min;
      if (value == "max")
        return Rasterizer::BlendOp::Max;
      optionsError(path, "expected add, subtract, reverse_subtract, min, or max");
    }

    Rasterizer::AlphaFunc alphaFuncFromString(const std::string& value, const std::string& path) {
      if (value == "never")
        return Rasterizer::AlphaFunc::Never;
      if (value == "less")
        return Rasterizer::AlphaFunc::Less;
      if (value == "equal")
        return Rasterizer::AlphaFunc::Equal;
      if (value == "less_equal")
        return Rasterizer::AlphaFunc::LessEqual;
      if (value == "greater")
        return Rasterizer::AlphaFunc::Greater;
      if (value == "greater_equal")
        return Rasterizer::AlphaFunc::GreaterEqual;
      if (value == "not_equal")
        return Rasterizer::AlphaFunc::NotEqual;
      if (value == "always")
        return Rasterizer::AlphaFunc::Always;
      optionsError(path, "unknown alpha function");
    }

    Rasterizer::ShadowFilterMode shadowFilterModeFromString(const std::string& value,
                                                            const std::string& path) {
      if (value == "pcf")
        return Rasterizer::ShadowFilterMode::PCF;
      if (value == "pcss")
        return Rasterizer::ShadowFilterMode::PCSS;
      optionsError(path, "expected pcf or pcss");
    }

    template<class T>
    std::optional<T> overrideOptional(std::optional<T> base, const std::optional<T>& override) {
      if (override)
        return override;
      return base;
    }
  }

  bool RenderRaytracerOptions::empty() const {
    return !m_maximumRecursionDepth && !m_maximumThreads && !m_queueSize && !m_integrator &&
           !m_tracingBackend && !m_tracingExecution && !m_intersectionBackend &&
           !m_russianRouletteDepth && !m_directLightSamples && !m_sampler && !m_samplesPerPixel &&
           !m_gpuPrimarySampleChunkSize && !m_samplingSeed && !m_sampleStreamMode && !m_viewPlane &&
           !m_convergenceEnabled && !m_convergenceActiveSampleFractionThreshold &&
           !m_convergenceRadianceDeltaRmsThreshold && !m_adaptiveSamplingEnabled &&
           !m_adaptiveMinimumSamples && !m_adaptiveStddevThreshold && !m_denoiser &&
           !m_denoiseRadius && !m_denoiseColorSigma;
  }

  QJsonObject RenderRaytracerOptions::toJson() const {
    return beautyPassState().toJson();
  }

  RenderRaytracerOptions RenderRaytracerOptions::fromJson(const QJsonObject& object,
                                                          const std::string& path) {
    const RaytracerBeautyPassState state = RaytracerBeautyPassState::fromJson(object, path);
    RenderRaytracerOptions options;
    if (state.maximumRecursionDepth())
      options.setMaximumRecursionDepth(*state.maximumRecursionDepth());
    if (state.maximumThreads())
      options.setMaximumThreads(*state.maximumThreads());
    if (state.queueSize())
      options.setQueueSize(*state.queueSize());
    if (state.integrator())
      options.setIntegrator(*state.integrator());
    if (state.tracingBackend())
      options.setTracingBackend(*state.tracingBackend());
    if (state.tracingExecution())
      options.setTracingExecution(*state.tracingExecution());
    if (state.intersectionBackend())
      options.setIntersectionBackend(*state.intersectionBackend());
    if (state.russianRouletteDepth())
      options.setRussianRouletteDepth(*state.russianRouletteDepth());
    if (state.directLightSamples())
      options.setDirectLightSamples(*state.directLightSamples());
    if (state.gpuPrimarySampleChunkSize())
      options.setGpuPrimarySampleChunkSize(*state.gpuPrimarySampleChunkSize());
    if (state.sampler())
      options.setSampler(*state.sampler());
    if (state.samplesPerPixel())
      options.setSamplesPerPixel(*state.samplesPerPixel());
    if (state.samplingSeed())
      options.setSamplingSeed(*state.samplingSeed());
    if (state.sampleStreamMode())
      options.setSampleStreamMode(*state.sampleStreamMode());
    if (state.viewPlane())
      options.setViewPlane(*state.viewPlane());
    if (state.convergenceEnabled())
      options.setConvergenceEnabled(*state.convergenceEnabled());
    if (state.convergenceActiveSampleFractionThreshold()) {
      options.setConvergenceActiveSampleFractionThreshold(
        *state.convergenceActiveSampleFractionThreshold());
    }
    if (state.convergenceRadianceDeltaRmsThreshold())
      options.setConvergenceRadianceDeltaRmsThreshold(
        *state.convergenceRadianceDeltaRmsThreshold());
    if (state.adaptiveSamplingEnabled())
      options.setAdaptiveSamplingEnabled(*state.adaptiveSamplingEnabled());
    if (state.adaptiveMinimumSamples())
      options.setAdaptiveMinimumSamples(*state.adaptiveMinimumSamples());
    if (state.adaptiveStddevThreshold())
      options.setAdaptiveStddevThreshold(*state.adaptiveStddevThreshold());
    if (state.denoiser())
      options.setDenoiser(*state.denoiser());
    if (state.denoiseRadius())
      options.setDenoiseRadius(*state.denoiseRadius());
    if (state.denoiseColorSigma())
      options.setDenoiseColorSigma(*state.denoiseColorSigma());
    return options;
  }

  RenderRaytracerOptions
  RenderRaytracerOptions::mergedWith(const RenderRaytracerOptions& overrides) const {
    RenderRaytracerOptions result = *this;
    result.m_maximumRecursionDepth =
      overrideOptional(result.m_maximumRecursionDepth, overrides.m_maximumRecursionDepth);
    result.m_maximumThreads = overrideOptional(result.m_maximumThreads, overrides.m_maximumThreads);
    result.m_queueSize = overrideOptional(result.m_queueSize, overrides.m_queueSize);
    result.m_integrator = overrideOptional(result.m_integrator, overrides.m_integrator);
    result.m_tracingBackend = overrideOptional(result.m_tracingBackend, overrides.m_tracingBackend);
    result.m_tracingExecution =
      overrideOptional(result.m_tracingExecution, overrides.m_tracingExecution);
    result.m_intersectionBackend =
      overrideOptional(result.m_intersectionBackend, overrides.m_intersectionBackend);
    result.m_russianRouletteDepth =
      overrideOptional(result.m_russianRouletteDepth, overrides.m_russianRouletteDepth);
    result.m_directLightSamples =
      overrideOptional(result.m_directLightSamples, overrides.m_directLightSamples);
    result.m_gpuPrimarySampleChunkSize =
      overrideOptional(result.m_gpuPrimarySampleChunkSize, overrides.m_gpuPrimarySampleChunkSize);
    result.m_sampler = overrideOptional(result.m_sampler, overrides.m_sampler);
    result.m_samplesPerPixel =
      overrideOptional(result.m_samplesPerPixel, overrides.m_samplesPerPixel);
    result.m_samplingSeed = overrideOptional(result.m_samplingSeed, overrides.m_samplingSeed);
    result.m_sampleStreamMode =
      overrideOptional(result.m_sampleStreamMode, overrides.m_sampleStreamMode);
    result.m_viewPlane = overrideOptional(result.m_viewPlane, overrides.m_viewPlane);
    result.m_convergenceEnabled =
      overrideOptional(result.m_convergenceEnabled, overrides.m_convergenceEnabled);
    result.m_convergenceActiveSampleFractionThreshold =
      overrideOptional(result.m_convergenceActiveSampleFractionThreshold,
                       overrides.m_convergenceActiveSampleFractionThreshold);
    result.m_convergenceRadianceDeltaRmsThreshold =
      overrideOptional(result.m_convergenceRadianceDeltaRmsThreshold,
                       overrides.m_convergenceRadianceDeltaRmsThreshold);
    result.m_adaptiveSamplingEnabled =
      overrideOptional(result.m_adaptiveSamplingEnabled, overrides.m_adaptiveSamplingEnabled);
    result.m_adaptiveMinimumSamples =
      overrideOptional(result.m_adaptiveMinimumSamples, overrides.m_adaptiveMinimumSamples);
    result.m_adaptiveStddevThreshold =
      overrideOptional(result.m_adaptiveStddevThreshold, overrides.m_adaptiveStddevThreshold);
    result.m_denoiser = overrideOptional(result.m_denoiser, overrides.m_denoiser);
    result.m_denoiseRadius = overrideOptional(result.m_denoiseRadius, overrides.m_denoiseRadius);
    result.m_denoiseColorSigma =
      overrideOptional(result.m_denoiseColorSigma, overrides.m_denoiseColorSigma);
    return result;
  }

  RaytracerBeautyPassState RenderRaytracerOptions::beautyPassState() const {
    RaytracerBeautyPassState state;
    if (m_maximumRecursionDepth)
      state.setMaximumRecursionDepth(*m_maximumRecursionDepth);
    if (m_maximumThreads)
      state.setMaximumThreads(*m_maximumThreads);
    if (m_queueSize)
      state.setQueueSize(*m_queueSize);
    if (m_integrator)
      state.setIntegrator(*m_integrator);
    if (m_tracingBackend)
      state.setTracingBackend(*m_tracingBackend);
    if (m_tracingExecution)
      state.setTracingExecution(*m_tracingExecution);
    if (m_intersectionBackend)
      state.setIntersectionBackend(*m_intersectionBackend);
    if (m_russianRouletteDepth)
      state.setRussianRouletteDepth(*m_russianRouletteDepth);
    if (m_directLightSamples)
      state.setDirectLightSamples(*m_directLightSamples);
    if (m_gpuPrimarySampleChunkSize)
      state.setGpuPrimarySampleChunkSize(*m_gpuPrimarySampleChunkSize);
    if (m_sampler)
      state.setSampler(*m_sampler);
    if (m_samplesPerPixel)
      state.setSamplesPerPixel(*m_samplesPerPixel);
    if (m_samplingSeed)
      state.setSamplingSeed(*m_samplingSeed);
    if (m_sampleStreamMode)
      state.setSampleStreamMode(*m_sampleStreamMode);
    if (m_viewPlane)
      state.setViewPlane(*m_viewPlane);
    if (m_convergenceEnabled)
      state.setConvergenceEnabled(*m_convergenceEnabled);
    if (m_convergenceActiveSampleFractionThreshold)
      state.setConvergenceActiveSampleFractionThreshold(
        *m_convergenceActiveSampleFractionThreshold);
    if (m_convergenceRadianceDeltaRmsThreshold)
      state.setConvergenceRadianceDeltaRmsThreshold(*m_convergenceRadianceDeltaRmsThreshold);
    if (m_adaptiveSamplingEnabled)
      state.setAdaptiveSamplingEnabled(*m_adaptiveSamplingEnabled);
    if (m_adaptiveMinimumSamples)
      state.setAdaptiveMinimumSamples(*m_adaptiveMinimumSamples);
    if (m_adaptiveStddevThreshold)
      state.setAdaptiveStddevThreshold(*m_adaptiveStddevThreshold);
    if (m_denoiser)
      state.setDenoiser(*m_denoiser);
    if (m_denoiseRadius)
      state.setDenoiseRadius(*m_denoiseRadius);
    if (m_denoiseColorSigma)
      state.setDenoiseColorSigma(*m_denoiseColorSigma);
    return state;
  }

  void RenderRaytracerOptions::setMaximumRecursionDepth(int depth) {
    m_maximumRecursionDepth = std::max(1, depth);
  }

  void RenderRaytracerOptions::setMaximumThreads(int threads) {
    m_maximumThreads = std::max(1, threads);
  }

  void RenderRaytracerOptions::setQueueSize(int queueSize) {
    m_queueSize = std::max(1, queueSize);
  }

  void RenderRaytracerOptions::setIntegrator(std::string integrator) {
    RaytracerBeautyPassState state;
    state.setIntegrator(std::move(integrator));
    m_integrator = state.integrator();
  }

  void RenderRaytracerOptions::setTracingBackend(std::string backend) {
    RaytracerBeautyPassState state;
    state.setTracingBackend(std::move(backend));
    m_tracingBackend = state.tracingBackend();
  }

  void
  RenderRaytracerOptions::setTracingBackend(render::WavefrontIntersectionBackendChoice backend) {
    m_tracingBackend = backend;
  }

  void RenderRaytracerOptions::setTracingExecution(TracingExecutionPreference preference) {
    m_tracingExecution = preference;
  }

  void RenderRaytracerOptions::setTracingExecution(std::string preference) {
    RaytracerBeautyPassState state;
    state.setTracingExecution(std::move(preference));
    m_tracingExecution = state.tracingExecution();
  }

  void RenderRaytracerOptions::setIntersectionBackend(std::string backend) {
    RaytracerBeautyPassState state;
    state.setIntersectionBackend(std::move(backend));
    m_intersectionBackend = state.intersectionBackend();
  }

  void RenderRaytracerOptions::setIntersectionBackend(
    render::WavefrontIntersectionBackendChoice backend) {
    m_intersectionBackend = backend;
  }

  void RenderRaytracerOptions::setRussianRouletteDepth(int depth) {
    m_russianRouletteDepth = std::max(1, depth);
  }

  void RenderRaytracerOptions::setDirectLightSamples(int samples) {
    m_directLightSamples = std::max(1, samples);
  }

  void RenderRaytracerOptions::setGpuPrimarySampleChunkSize(int samples) {
    m_gpuPrimarySampleChunkSize = std::max(0, samples);
  }

  void RenderRaytracerOptions::setSampler(std::string sampler) {
    m_sampler = std::move(sampler);
  }

  void RenderRaytracerOptions::clearSampler() {
    m_sampler.reset();
  }

  void RenderRaytracerOptions::setSamplesPerPixel(int samples) {
    m_samplesPerPixel = std::max(1, samples);
  }

  void RenderRaytracerOptions::setSamplingSeed(std::uint64_t seed) {
    RaytracerBeautyPassState state;
    state.setSamplingSeed(seed);
    m_samplingSeed = state.samplingSeed();
  }

  void RenderRaytracerOptions::setSampleStreamMode(std::string mode) {
    RaytracerBeautyPassState state;
    state.setSampleStreamMode(std::move(mode));
    m_sampleStreamMode = state.sampleStreamMode();
  }

  void RenderRaytracerOptions::setViewPlane(std::string viewPlane) {
    m_viewPlane = std::move(viewPlane);
  }

  void RenderRaytracerOptions::setConvergenceEnabled(bool enabled) {
    m_convergenceEnabled = enabled;
  }

  void RenderRaytracerOptions::setConvergenceActiveSampleFractionThreshold(double fraction) {
    m_convergenceActiveSampleFractionThreshold = std::clamp(fraction, 0.0, 1.0);
  }

  void RenderRaytracerOptions::setConvergenceRadianceDeltaRmsThreshold(double threshold) {
    m_convergenceRadianceDeltaRmsThreshold = std::max(0.0, threshold);
  }

  void RenderRaytracerOptions::setAdaptiveSamplingEnabled(bool enabled) {
    m_adaptiveSamplingEnabled = enabled;
  }

  void RenderRaytracerOptions::setAdaptiveMinimumSamples(int samples) {
    m_adaptiveMinimumSamples = std::max(1, samples);
  }

  void RenderRaytracerOptions::setAdaptiveStddevThreshold(double threshold) {
    m_adaptiveStddevThreshold = std::max(0.0, threshold);
  }

  void RenderRaytracerOptions::setDenoiser(std::string denoiser) {
    RaytracerBeautyPassState state;
    state.setDenoiser(std::move(denoiser));
    m_denoiser = state.denoiser();
  }

  void RenderRaytracerOptions::setDenoiseRadius(int radius) {
    m_denoiseRadius = std::max(0, radius);
  }

  void RenderRaytracerOptions::setDenoiseColorSigma(double sigma) {
    m_denoiseColorSigma = std::max(0.0, sigma);
  }

  std::optional<int> RenderRaytracerOptions::maximumRecursionDepth() const {
    return m_maximumRecursionDepth;
  }

  std::optional<int> RenderRaytracerOptions::maximumThreads() const {
    return m_maximumThreads;
  }

  std::optional<int> RenderRaytracerOptions::queueSize() const {
    return m_queueSize;
  }

  std::optional<std::string> RenderRaytracerOptions::integrator() const {
    return m_integrator;
  }

  std::optional<render::WavefrontIntersectionBackendChoice>
  RenderRaytracerOptions::tracingBackend() const {
    return m_tracingBackend;
  }

  std::optional<TracingExecutionPreference> RenderRaytracerOptions::tracingExecution() const {
    return m_tracingExecution;
  }

  std::optional<render::WavefrontIntersectionBackendChoice>
  RenderRaytracerOptions::intersectionBackend() const {
    return m_intersectionBackend;
  }

  std::optional<int> RenderRaytracerOptions::russianRouletteDepth() const {
    return m_russianRouletteDepth;
  }

  std::optional<int> RenderRaytracerOptions::directLightSamples() const {
    return m_directLightSamples;
  }

  std::optional<int> RenderRaytracerOptions::gpuPrimarySampleChunkSize() const {
    return m_gpuPrimarySampleChunkSize;
  }

  std::optional<std::string> RenderRaytracerOptions::sampler() const {
    return m_sampler;
  }

  std::optional<int> RenderRaytracerOptions::samplesPerPixel() const {
    return m_samplesPerPixel;
  }

  std::optional<std::uint64_t> RenderRaytracerOptions::samplingSeed() const {
    return m_samplingSeed;
  }

  std::optional<std::string> RenderRaytracerOptions::sampleStreamMode() const {
    return m_sampleStreamMode;
  }

  std::optional<std::string> RenderRaytracerOptions::viewPlane() const {
    return m_viewPlane;
  }

  std::optional<bool> RenderRaytracerOptions::convergenceEnabled() const {
    return m_convergenceEnabled;
  }

  std::optional<double> RenderRaytracerOptions::convergenceActiveSampleFractionThreshold() const {
    return m_convergenceActiveSampleFractionThreshold;
  }

  std::optional<double> RenderRaytracerOptions::convergenceRadianceDeltaRmsThreshold() const {
    return m_convergenceRadianceDeltaRmsThreshold;
  }

  std::optional<bool> RenderRaytracerOptions::adaptiveSamplingEnabled() const {
    return m_adaptiveSamplingEnabled;
  }

  std::optional<int> RenderRaytracerOptions::adaptiveMinimumSamples() const {
    return m_adaptiveMinimumSamples;
  }

  std::optional<double> RenderRaytracerOptions::adaptiveStddevThreshold() const {
    return m_adaptiveStddevThreshold;
  }

  std::optional<std::string> RenderRaytracerOptions::denoiser() const {
    return m_denoiser;
  }

  std::optional<int> RenderRaytracerOptions::denoiseRadius() const {
    return m_denoiseRadius;
  }

  std::optional<double> RenderRaytracerOptions::denoiseColorSigma() const {
    return m_denoiseColorSigma;
  }

  bool RenderRasterizerOptions::empty() const {
    return toJson().isEmpty();
  }

  QJsonObject RenderRasterizerOptions::toJson() const {
    QJsonObject object;

    QJsonObject execution;
    if (m_maximumThreads)
      execution["threads"] = *m_maximumThreads;
    if (m_queueSize)
      execution["queueSize"] = *m_queueSize;
    if (m_backend && !m_backend->isCPU())
      execution["backend"] = qstr(m_backend->id());
    if (!execution.isEmpty())
      object["execution"] = execution;

    QJsonObject geometry;
    if (m_lod)
      geometry["lod"] = *m_lod;
    if (m_tessellationQuality)
      geometry["quality"] = qstr(*m_tessellationQuality);
    if (m_maximumScreenSpaceError)
      geometry["maxScreenSpaceError"] = *m_maximumScreenSpaceError;
    if (m_cullMode)
      geometry["cullMode"] = qstr(*m_cullMode);
    if (m_visibilityCulling)
      geometry["visibilityCulling"] =
        qstr(RenderRasterizerOptions::visibilityCullingName(*m_visibilityCulling));
    if (!geometry.isEmpty())
      object["geometry"] = geometry;

    QJsonObject depthPrepass;
    if (m_depthPrepass)
      depthPrepass["mode"] = qstr(*m_depthPrepass);
    if (!depthPrepass.isEmpty())
      object["depthPrepass"] = depthPrepass;

    QJsonObject sampling;
    if (m_msaaSamples)
      sampling["msaaSamples"] = *m_msaaSamples;
    if (m_msaaShadingMode)
      sampling["msaaShadingMode"] = qstr(*m_msaaShadingMode);
    if (!sampling.isEmpty())
      object["sampling"] = sampling;

    QJsonObject framebuffer;
    if (m_viewportRect)
      framebuffer["viewport"] = detail::rectToJson(*m_viewportRect);
    if (m_scissorRect)
      framebuffer["scissor"] = detail::rectToJson(*m_scissorRect);
    if (m_depthBias)
      framebuffer["depthBias"] = *m_depthBias;
    if (m_colorWriteMask)
      framebuffer["colorWriteMask"] = qstr(colorWriteMaskString(*m_colorWriteMask));
    if (m_blendingEnabled)
      framebuffer["blending"] = *m_blendingEnabled;
    if (m_sourceBlendFactor)
      framebuffer["blendSource"] = qstr(*m_sourceBlendFactor);
    if (m_destinationBlendFactor)
      framebuffer["blendDestination"] = qstr(*m_destinationBlendFactor);
    if (m_blendOp)
      framebuffer["blendOp"] = qstr(*m_blendOp);
    if (m_blendConstantColor)
      framebuffer["blendConstantColor"] = detail::colorToJson(*m_blendConstantColor);
    if (m_blendConstantAlpha)
      framebuffer["blendConstantAlpha"] = *m_blendConstantAlpha;
    if (m_alphaTestEnabled)
      framebuffer["alphaTest"] = *m_alphaTestEnabled;
    if (m_alphaFunc)
      framebuffer["alphaFunc"] = qstr(*m_alphaFunc);
    if (m_alphaReference)
      framebuffer["alphaReference"] = *m_alphaReference;
    if (!framebuffer.isEmpty())
      object["framebuffer"] = framebuffer;

    QJsonObject shadows;
    if (m_shadowMapSize)
      shadows["mapSize"] = *m_shadowMapSize;
    if (m_shadowCascadeCount)
      shadows["cascadeCount"] = *m_shadowCascadeCount;
    if (m_shadowCascadeSplitLambda)
      shadows["cascadeSplitLambda"] = *m_shadowCascadeSplitLambda;
    if (m_shadowBias)
      shadows["bias"] = *m_shadowBias;
    if (m_shadowSlopeBias)
      shadows["slopeBias"] = *m_shadowSlopeBias;
    if (m_shadowFilterRadius)
      shadows["filterRadius"] = *m_shadowFilterRadius;
    if (m_shadowFilterMode)
      shadows["filterMode"] = qstr(*m_shadowFilterMode);
    if (m_shadowMode)
      shadows["mode"] = qstr(shadowModeName(*m_shadowMode));
    if (!shadows.isEmpty())
      object["shadows"] = shadows;

    return object;
  }

  RenderRasterizerOptions RenderRasterizerOptions::fromJson(const QJsonObject& object,
                                                            const std::string& path) {
    detail::rejectUnknownFields(
      object, path,
      {"execution", "geometry", "sampling", "depthPrepass", "framebuffer", "shadows"}, optionsError);

    RenderRasterizerOptions options;
    const QJsonObject execution = detail::objectField(object, "execution", path, optionsError);
    detail::rejectUnknownFields(execution, path + ".execution", {"threads", "queueSize", "backend"}, optionsError);
    if (detail::hasField(execution, "threads"))
      options.setMaximumThreads(detail::intField(execution, "threads", path + ".execution", optionsError));
    if (detail::hasField(execution, "queueSize"))
      options.setQueueSize(detail::intField(execution, "queueSize", path + ".execution", optionsError));
    if (detail::hasField(execution, "backend"))
      options.setBackend(engine::raster::RasterBackend::fromString(
        detail::stringField(execution, "backend", path + ".execution", optionsError), path + ".execution.backend"));

    const QJsonObject geometry = detail::objectField(object, "geometry", path, optionsError);
    detail::rejectUnknownFields(geometry, path + ".geometry",
                        {"lod", "quality", "maxScreenSpaceError", "cullMode", "visibilityCulling"}, optionsError);
    if (detail::hasField(geometry, "lod"))
      options.setLod(detail::intField(geometry, "lod", path + ".geometry", optionsError));
    if (detail::hasField(geometry, "quality"))
      options.setTessellationQuality(detail::stringField(geometry, "quality", path + ".geometry", optionsError));
    if (detail::hasField(geometry, "maxScreenSpaceError"))
      options.setMaximumScreenSpaceError(
        detail::doubleField(geometry, "maxScreenSpaceError", path + ".geometry", optionsError));
    if (detail::hasField(geometry, "cullMode"))
      options.setCullMode(detail::stringField(geometry, "cullMode", path + ".geometry", optionsError));
    if (detail::hasField(geometry, "visibilityCulling"))
      options.setVisibilityCulling(detail::stringField(geometry, "visibilityCulling", path + ".geometry", optionsError));

    const QJsonObject depthPrepass = detail::objectField(object, "depthPrepass", path, optionsError);
    detail::rejectUnknownFields(depthPrepass, path + ".depthPrepass", {"mode"}, optionsError);
    if (detail::hasField(depthPrepass, "mode"))
      options.setDepthPrepass(detail::stringField(depthPrepass, "mode", path + ".depthPrepass", optionsError));

    const QJsonObject sampling = detail::objectField(object, "sampling", path, optionsError);
    detail::rejectUnknownFields(sampling, path + ".sampling", {"msaaSamples", "msaaShadingMode"}, optionsError);
    if (detail::hasField(sampling, "msaaSamples"))
      options.setMSAASamples(detail::intField(sampling, "msaaSamples", path + ".sampling", optionsError));
    if (detail::hasField(sampling, "msaaShadingMode"))
      options.setMSAAShadingMode(detail::stringField(sampling, "msaaShadingMode", path + ".sampling", optionsError));

    const QJsonObject framebuffer = detail::objectField(object, "framebuffer", path, optionsError);
    detail::rejectUnknownFields(framebuffer, path + ".framebuffer",
                        {"viewport", "scissor", "depthBias", "colorWriteMask", "blending",
                         "blendSource", "blendDestination", "blendOp", "blendConstantColor",
                         "blendConstantAlpha", "alphaTest", "alphaFunc", "alphaReference"}, optionsError);
    if (detail::hasField(framebuffer, "viewport"))
      options.setViewportRect(
        detail::rectFromJson(framebuffer, "viewport", path + ".framebuffer", optionsError));
    if (detail::hasField(framebuffer, "scissor"))
      options.setScissorRect(
        detail::rectFromJson(framebuffer, "scissor", path + ".framebuffer", optionsError));
    if (detail::hasField(framebuffer, "depthBias"))
      options.setDepthBias(detail::doubleField(framebuffer, "depthBias", path + ".framebuffer", optionsError));
    if (detail::hasField(framebuffer, "colorWriteMask"))
      options.setColorWriteMask(
        colorWriteMaskFromString(detail::stringField(framebuffer, "colorWriteMask", path + ".framebuffer", optionsError),
                                 path + ".framebuffer.colorWriteMask"));
    if (detail::hasField(framebuffer, "blending"))
      options.setBlendingEnabled(detail::boolField(framebuffer, "blending", path + ".framebuffer", optionsError));
    if (detail::hasField(framebuffer, "blendSource") || detail::hasField(framebuffer, "blendDestination")) {
      options.setBlendFactors(
        detail::hasField(framebuffer, "blendSource")
          ? detail::stringField(framebuffer, "blendSource", path + ".framebuffer", optionsError)
          : "one",
        detail::hasField(framebuffer, "blendDestination")
          ? detail::stringField(framebuffer, "blendDestination", path + ".framebuffer", optionsError)
          : "zero");
    }
    if (detail::hasField(framebuffer, "blendOp"))
      options.setBlendOp(detail::stringField(framebuffer, "blendOp", path + ".framebuffer", optionsError));
    if (detail::hasField(framebuffer, "blendConstantColor") ||
        detail::hasField(framebuffer, "blendConstantAlpha")) {
      options.setBlendConstant(
        detail::hasField(framebuffer, "blendConstantColor")
          ? detail::colorFromJson(framebuffer, "blendConstantColor", path + ".framebuffer",
                                  optionsError)
          : Colord::white(),
        detail::hasField(framebuffer, "blendConstantAlpha")
          ? detail::doubleField(framebuffer, "blendConstantAlpha", path + ".framebuffer", optionsError)
          : 1.0);
    }
    if (detail::hasField(framebuffer, "alphaTest"))
      options.setAlphaTestEnabled(detail::boolField(framebuffer, "alphaTest", path + ".framebuffer", optionsError));
    if (detail::hasField(framebuffer, "alphaFunc") || detail::hasField(framebuffer, "alphaReference")) {
      options.setAlphaFunc(detail::hasField(framebuffer, "alphaFunc")
                             ? detail::stringField(framebuffer, "alphaFunc", path + ".framebuffer", optionsError)
                             : "always",
                           detail::hasField(framebuffer, "alphaReference")
                             ? detail::doubleField(framebuffer, "alphaReference", path + ".framebuffer", optionsError)
                             : 0.0);
    }

    const QJsonObject shadows = detail::objectField(object, "shadows", path, optionsError);
    detail::rejectUnknownFields(shadows, path + ".shadows",
                        {"mapSize", "cascadeCount", "cascadeSplitLambda", "bias", "slopeBias",
                         "filterRadius", "filterMode", "mode"}, optionsError);
    if (detail::hasField(shadows, "mapSize"))
      options.setShadowMapSize(detail::intField(shadows, "mapSize", path + ".shadows", optionsError));
    if (detail::hasField(shadows, "cascadeCount"))
      options.setShadowCascadeCount(detail::intField(shadows, "cascadeCount", path + ".shadows", optionsError));
    if (detail::hasField(shadows, "cascadeSplitLambda"))
      options.setShadowCascadeSplitLambda(
        detail::doubleField(shadows, "cascadeSplitLambda", path + ".shadows", optionsError));
    if (detail::hasField(shadows, "bias"))
      options.setShadowBias(detail::doubleField(shadows, "bias", path + ".shadows", optionsError));
    if (detail::hasField(shadows, "slopeBias"))
      options.setShadowSlopeBias(detail::doubleField(shadows, "slopeBias", path + ".shadows", optionsError));
    if (detail::hasField(shadows, "filterRadius"))
      options.setShadowFilterRadius(detail::intField(shadows, "filterRadius", path + ".shadows", optionsError));
    if (detail::hasField(shadows, "filterMode"))
      options.setShadowFilterMode(detail::stringField(shadows, "filterMode", path + ".shadows", optionsError));
    if (detail::hasField(shadows, "mode"))
      options.setShadowMode(detail::stringField(shadows, "mode", path + ".shadows", optionsError));

    return options;
  }

  RenderRasterizerOptions
  RenderRasterizerOptions::mergedWith(const RenderRasterizerOptions& overrides) const {
    RenderRasterizerOptions result = *this;
    result.m_maximumThreads = overrideOptional(result.m_maximumThreads, overrides.m_maximumThreads);
    result.m_queueSize = overrideOptional(result.m_queueSize, overrides.m_queueSize);
    result.m_backend = overrideOptional(result.m_backend, overrides.m_backend);
    result.m_lod = overrideOptional(result.m_lod, overrides.m_lod);
    result.m_tessellationQuality =
      overrideOptional(result.m_tessellationQuality, overrides.m_tessellationQuality);
    result.m_maximumScreenSpaceError =
      overrideOptional(result.m_maximumScreenSpaceError, overrides.m_maximumScreenSpaceError);
    result.m_cullMode = overrideOptional(result.m_cullMode, overrides.m_cullMode);
    result.m_visibilityCulling =
      overrideOptional(result.m_visibilityCulling, overrides.m_visibilityCulling);
    result.m_depthPrepass = overrideOptional(result.m_depthPrepass, overrides.m_depthPrepass);
    result.m_msaaSamples = overrideOptional(result.m_msaaSamples, overrides.m_msaaSamples);
    result.m_msaaShadingMode =
      overrideOptional(result.m_msaaShadingMode, overrides.m_msaaShadingMode);
    result.m_viewportRect = overrideOptional(result.m_viewportRect, overrides.m_viewportRect);
    result.m_scissorRect = overrideOptional(result.m_scissorRect, overrides.m_scissorRect);
    result.m_depthBias = overrideOptional(result.m_depthBias, overrides.m_depthBias);
    result.m_colorWriteMask = overrideOptional(result.m_colorWriteMask, overrides.m_colorWriteMask);
    result.m_blendingEnabled =
      overrideOptional(result.m_blendingEnabled, overrides.m_blendingEnabled);
    result.m_sourceBlendFactor =
      overrideOptional(result.m_sourceBlendFactor, overrides.m_sourceBlendFactor);
    result.m_destinationBlendFactor =
      overrideOptional(result.m_destinationBlendFactor, overrides.m_destinationBlendFactor);
    result.m_blendOp = overrideOptional(result.m_blendOp, overrides.m_blendOp);
    result.m_blendConstantColor =
      overrideOptional(result.m_blendConstantColor, overrides.m_blendConstantColor);
    result.m_blendConstantAlpha =
      overrideOptional(result.m_blendConstantAlpha, overrides.m_blendConstantAlpha);
    result.m_alphaTestEnabled =
      overrideOptional(result.m_alphaTestEnabled, overrides.m_alphaTestEnabled);
    result.m_alphaFunc = overrideOptional(result.m_alphaFunc, overrides.m_alphaFunc);
    result.m_alphaReference = overrideOptional(result.m_alphaReference, overrides.m_alphaReference);
    result.m_shadowMapSize = overrideOptional(result.m_shadowMapSize, overrides.m_shadowMapSize);
    result.m_shadowCascadeCount =
      overrideOptional(result.m_shadowCascadeCount, overrides.m_shadowCascadeCount);
    result.m_shadowCascadeSplitLambda =
      overrideOptional(result.m_shadowCascadeSplitLambda, overrides.m_shadowCascadeSplitLambda);
    result.m_shadowBias = overrideOptional(result.m_shadowBias, overrides.m_shadowBias);
    result.m_shadowSlopeBias =
      overrideOptional(result.m_shadowSlopeBias, overrides.m_shadowSlopeBias);
    result.m_shadowFilterRadius =
      overrideOptional(result.m_shadowFilterRadius, overrides.m_shadowFilterRadius);
    result.m_shadowFilterMode =
      overrideOptional(result.m_shadowFilterMode, overrides.m_shadowFilterMode);
    result.m_shadowMode = overrideOptional(result.m_shadowMode, overrides.m_shadowMode);
    return result;
  }

  RasterBeautyPassState
  RenderRasterizerOptions::beautyPassState(int targetSampleCount, RenderPostProcessAA aa,
                                           bool includeImagePostProcessAA,
                                           bool includeShadowMapEnable) const {
    RasterBeautyPassState state;
    if (targetSampleCount > 1)
      state.sampling().setMSAASamples(targetSampleCount);
    if (m_maximumThreads)
      state.execution().setMaximumThreads(*m_maximumThreads);
    if (m_queueSize)
      state.execution().setQueueSize(*m_queueSize);
    if (m_backend)
      state.execution().setBackend(*m_backend);
    if (m_lod)
      state.geometry().setLod(*m_lod);
    if (m_tessellationQuality) {
      state.geometry().setTessellationQuality(
        tessellationQualityFromString(*m_tessellationQuality, "rasterizer.geometry.quality"));
    }
    if (m_maximumScreenSpaceError)
      state.geometry().setMaximumScreenSpaceError(*m_maximumScreenSpaceError);
    if (m_cullMode)
      state.geometry().setCullMode(cullModeFromString(*m_cullMode, "rasterizer.geometry.cullMode"));
    if (m_msaaSamples)
      state.sampling().setMSAASamples(*m_msaaSamples);
    if (m_msaaShadingMode) {
      state.sampling().setMSAAShadingMode(
        msaaShadingModeFromString(*m_msaaShadingMode, "rasterizer.sampling.msaaShadingMode"));
    } else if (m_backend && m_backend->isOpenGL() && state.sampling().msaaSamples() > 1) {
      state.sampling().setMSAAShadingMode(Rasterizer::MSAAShadingMode::PerFragment);
    }
    if (includeImagePostProcessAA || aa == RenderPostProcessAA::TAA)
      state.sampling().setPostProcessAA(rasterPostProcessAA(aa));
    if (m_depthPrepass) {
      if (*m_depthPrepass == "on") {
        state.depthPrepass().setMode(Rasterizer::DepthPrepassMode::On);
      } else if (*m_depthPrepass == "auto") {
        state.depthPrepass().setMode(Rasterizer::DepthPrepassMode::Auto);
      } else if (*m_depthPrepass == "off") {
        state.depthPrepass().setMode(Rasterizer::DepthPrepassMode::Off);
      } else {
        optionsError("rasterizer.depthPrepass.mode", "expected off, on, or auto");
      }
    }
    if (m_viewportRect)
      state.framebuffer().setViewportRect(*m_viewportRect);
    if (m_scissorRect)
      state.framebuffer().setScissorRect(*m_scissorRect);
    if (m_depthBias)
      state.framebuffer().setDepthBias(*m_depthBias);
    if (m_colorWriteMask)
      state.framebuffer().setColorWriteMask(*m_colorWriteMask);
    if (m_blendingEnabled)
      state.framebuffer().setBlendingEnabled(*m_blendingEnabled);
    if (m_sourceBlendFactor || m_destinationBlendFactor) {
      state.framebuffer().setBlendFactors(
        blendFactorFromString(m_sourceBlendFactor.value_or("one"),
                              "rasterizer.framebuffer.blendSource"),
        blendFactorFromString(m_destinationBlendFactor.value_or("zero"),
                              "rasterizer.framebuffer.blendDestination"));
    }
    if (m_blendOp)
      state.framebuffer().setBlendOp(
        blendOpFromString(*m_blendOp, "rasterizer.framebuffer.blendOp"));
    if (m_blendConstantColor || m_blendConstantAlpha) {
      state.framebuffer().setBlendConstant(m_blendConstantColor.value_or(Colord::white()),
                                           m_blendConstantAlpha.value_or(1.0));
    }
    if (m_alphaTestEnabled)
      state.framebuffer().setAlphaTestEnabled(*m_alphaTestEnabled);
    if (m_alphaFunc || m_alphaReference) {
      state.framebuffer().setAlphaFunc(
        alphaFuncFromString(m_alphaFunc.value_or("always"), "rasterizer.framebuffer.alphaFunc"),
        m_alphaReference.value_or(0.0));
    }
    if (includeShadowMapEnable) {
      state.shadows().setShadowMapsEnabled(true);
      state.shadows().setShadowMapSize(m_shadowMapSize.value_or(256));
      if (m_shadowCascadeCount)
        state.shadows().setShadowCascadeCount(*m_shadowCascadeCount);
      if (m_shadowCascadeSplitLambda)
        state.shadows().setShadowCascadeSplitLambda(*m_shadowCascadeSplitLambda);
      if (m_shadowBias)
        state.shadows().setShadowBias(*m_shadowBias);
      if (m_shadowSlopeBias)
        state.shadows().setShadowSlopeBias(*m_shadowSlopeBias);
      if (m_shadowFilterRadius)
        state.shadows().setShadowFilterRadius(*m_shadowFilterRadius);
      if (m_shadowFilterMode) {
        state.shadows().setShadowFilterMode(
          shadowFilterModeFromString(*m_shadowFilterMode, "rasterizer.shadows.filterMode"));
      }
    }
    return state;
  }

  RasterShadowPassState RenderRasterizerOptions::shadowPassState() const {
    RasterShadowPassState state = RasterShadowPassState::previewDefaults();
    if (m_shadowMapSize)
      state.shadows().setShadowMapSize(*m_shadowMapSize);
    if (m_shadowCascadeCount)
      state.shadows().setShadowCascadeCount(*m_shadowCascadeCount);
    if (m_shadowCascadeSplitLambda)
      state.shadows().setShadowCascadeSplitLambda(*m_shadowCascadeSplitLambda);
    if (m_shadowBias)
      state.shadows().setShadowBias(*m_shadowBias);
    if (m_shadowSlopeBias)
      state.shadows().setShadowSlopeBias(*m_shadowSlopeBias);
    if (m_shadowFilterRadius)
      state.shadows().setShadowFilterRadius(*m_shadowFilterRadius);
    if (m_shadowFilterMode)
      state.shadows().setShadowFilterMode(
        shadowFilterModeFromString(*m_shadowFilterMode, "rasterizer.shadows.filterMode"));
    return state;
  }

  RasterVisibilityPassState RenderRasterizerOptions::visibilityPassState() const {
    RasterVisibilityPassState state;
    if (m_lod)
      state.geometry().setLod(*m_lod);
    if (m_tessellationQuality) {
      state.geometry().setTessellationQuality(
        tessellationQualityFromString(*m_tessellationQuality, "rasterizer.geometry.quality"));
    }
    if (m_maximumScreenSpaceError)
      state.geometry().setMaximumScreenSpaceError(*m_maximumScreenSpaceError);
    if (m_cullMode)
      state.geometry().setCullMode(cullModeFromString(*m_cullMode, "rasterizer.geometry.cullMode"));
    const RasterBeautyPassState beautyState =
      beautyPassState(1, RenderPostProcessAA::None, false, false);
    state.setFrontToBackOrderingEnabled(
      beautyState.framebuffer().supportsFrontToBackVisibilityOrdering());
    return state;
  }

  void RenderRasterizerOptions::setMaximumThreads(int threads) {
    m_maximumThreads = std::max(1, threads);
  }

  void RenderRasterizerOptions::setQueueSize(int queueSize) {
    m_queueSize = std::max(1, queueSize);
  }

  void RenderRasterizerOptions::setBackend(engine::raster::RasterBackend backend) {
    m_backend = backend;
  }

  void RenderRasterizerOptions::setBackend(std::string backend) {
    m_backend =
      engine::raster::RasterBackend::fromString(std::move(backend), "rasterizer.execution.backend");
  }

  void RenderRasterizerOptions::setLod(int lod) {
    m_lod = std::max(0, lod);
  }

  void RenderRasterizerOptions::setTessellationQuality(std::string quality) {
    tessellationQualityFromString(quality, "rasterizer.geometry.quality");
    m_tessellationQuality = std::move(quality);
  }

  void RenderRasterizerOptions::setMaximumScreenSpaceError(double pixels) {
    m_maximumScreenSpaceError = std::isfinite(pixels) ? std::max(0.0, pixels) : 0.0;
  }

  void RenderRasterizerOptions::setCullMode(std::string mode) {
    cullModeFromString(mode, "rasterizer.geometry.cullMode");
    m_cullMode = std::move(mode);
  }

  RenderVisibilityCulling
  RenderRasterizerOptions::visibilityCullingFromString(const std::string& value,
                                                       const std::string& path) {
    if (value == "off")
      return RenderVisibilityCulling::Off;
    if (value == "on")
      return RenderVisibilityCulling::On;
    if (value == "auto")
      return RenderVisibilityCulling::Auto;
    optionsError(path, "expected off, on, or auto");
  }

  const char* RenderRasterizerOptions::visibilityCullingName(RenderVisibilityCulling mode) {
    switch (mode) {
    case RenderVisibilityCulling::Off:
      return "off";
    case RenderVisibilityCulling::On:
      return "on";
    case RenderVisibilityCulling::Auto:
      return "auto";
    }
    return "off";
  }

  RenderRasterShadowMode RenderRasterizerOptions::shadowModeFromString(const std::string& value,
                                                                       const std::string& path) {
    if (value == "shadow_maps")
      return RenderRasterShadowMode::ShadowMaps;
    if (value == "ray_traced")
      return RenderRasterShadowMode::RayTraced;
    optionsError(path, "expected shadow_maps or ray_traced");
  }

  const char* RenderRasterizerOptions::shadowModeName(RenderRasterShadowMode mode) {
    if (mode == RenderRasterShadowMode::RayTraced)
      return "ray_traced";
    return "shadow_maps";
  }

  void RenderRasterizerOptions::setVisibilityCulling(RenderVisibilityCulling mode) {
    m_visibilityCulling = mode;
  }

  void RenderRasterizerOptions::setVisibilityCulling(std::string mode) {
    m_visibilityCulling =
      visibilityCullingFromString(mode, "rasterizer.geometry.visibilityCulling");
  }

  void RenderRasterizerOptions::setDepthPrepass(std::string mode) {
    if (mode != "off" && mode != "on" && mode != "auto")
      optionsError("rasterizer.depthPrepass.mode", "expected off, on, or auto");
    m_depthPrepass = std::move(mode);
  }

  void RenderRasterizerOptions::setMSAASamples(int samples) {
    if (samples <= 1) {
      m_msaaSamples = 1;
    } else if (samples <= 2) {
      m_msaaSamples = 2;
    } else if (samples <= 4) {
      m_msaaSamples = 4;
    } else {
      m_msaaSamples = 8;
    }
  }

  void RenderRasterizerOptions::setMSAAShadingMode(std::string mode) {
    msaaShadingModeFromString(mode, "rasterizer.sampling.msaaShadingMode");
    m_msaaShadingMode = std::move(mode);
  }

  void RenderRasterizerOptions::setViewportRect(const Recti& rect) {
    m_viewportRect = rect;
  }

  void RenderRasterizerOptions::setScissorRect(const Recti& rect) {
    m_scissorRect = rect;
  }

  void RenderRasterizerOptions::setDepthBias(double bias) {
    m_depthBias = std::isfinite(bias) ? bias : 0.0;
  }

  void RenderRasterizerOptions::setColorWriteMask(std::uint8_t mask) {
    m_colorWriteMask = mask & Rasterizer::ColorWriteAll;
  }

  void RenderRasterizerOptions::setBlendingEnabled(bool enabled) {
    m_blendingEnabled = enabled;
  }

  void RenderRasterizerOptions::setBlendFactors(std::string source, std::string destination) {
    blendFactorFromString(source, "rasterizer.framebuffer.blendSource");
    blendFactorFromString(destination, "rasterizer.framebuffer.blendDestination");
    m_sourceBlendFactor = std::move(source);
    m_destinationBlendFactor = std::move(destination);
  }

  void RenderRasterizerOptions::setBlendOp(std::string op) {
    blendOpFromString(op, "rasterizer.framebuffer.blendOp");
    m_blendOp = std::move(op);
  }

  void RenderRasterizerOptions::setBlendConstant(const Colord& color, double alpha) {
    m_blendConstantColor = color;
    m_blendConstantAlpha = std::isfinite(alpha) ? std::clamp(alpha, 0.0, 1.0) : 1.0;
  }

  void RenderRasterizerOptions::setAlphaTestEnabled(bool enabled) {
    m_alphaTestEnabled = enabled;
  }

  void RenderRasterizerOptions::setAlphaFunc(std::string func, double reference) {
    alphaFuncFromString(func, "rasterizer.framebuffer.alphaFunc");
    m_alphaFunc = std::move(func);
    m_alphaReference = std::isfinite(reference) ? std::clamp(reference, 0.0, 1.0) : 0.0;
  }

  void RenderRasterizerOptions::setShadowMapSize(int size) {
    m_shadowMapSize = std::max(1, size);
  }

  void RenderRasterizerOptions::setShadowCascadeCount(int count) {
    m_shadowCascadeCount = std::clamp(count, 1, 4);
  }

  void RenderRasterizerOptions::setShadowCascadeSplitLambda(double lambda) {
    m_shadowCascadeSplitLambda = std::isfinite(lambda) ? std::clamp(lambda, 0.0, 1.0) : 0.5;
  }

  void RenderRasterizerOptions::setShadowBias(double bias) {
    m_shadowBias = std::max(0.0, bias);
  }

  void RenderRasterizerOptions::setShadowSlopeBias(double bias) {
    m_shadowSlopeBias = std::max(0.0, bias);
  }

  void RenderRasterizerOptions::setShadowFilterRadius(int radius) {
    m_shadowFilterRadius = std::max(0, radius);
  }

  void RenderRasterizerOptions::setShadowFilterMode(std::string mode) {
    shadowFilterModeFromString(mode, "rasterizer.shadows.filterMode");
    m_shadowFilterMode = std::move(mode);
  }

  void RenderRasterizerOptions::setShadowMode(RenderRasterShadowMode mode) {
    m_shadowMode = mode;
  }

  void RenderRasterizerOptions::setShadowMode(std::string mode) {
    m_shadowMode = shadowModeFromString(mode, "rasterizer.shadows.mode");
  }

  std::optional<int> RenderRasterizerOptions::maximumThreads() const {
    return m_maximumThreads;
  }

  std::optional<int> RenderRasterizerOptions::queueSize() const {
    return m_queueSize;
  }

  std::optional<engine::raster::RasterBackend> RenderRasterizerOptions::backend() const {
    return m_backend;
  }

  std::optional<int> RenderRasterizerOptions::lod() const {
    return m_lod;
  }

  std::optional<std::string> RenderRasterizerOptions::tessellationQuality() const {
    return m_tessellationQuality;
  }

  std::optional<double> RenderRasterizerOptions::maximumScreenSpaceError() const {
    return m_maximumScreenSpaceError;
  }

  std::optional<std::string> RenderRasterizerOptions::cullMode() const {
    return m_cullMode;
  }

  std::optional<RenderVisibilityCulling> RenderRasterizerOptions::visibilityCulling() const {
    return m_visibilityCulling;
  }

  std::optional<std::string> RenderRasterizerOptions::depthPrepass() const {
    return m_depthPrepass;
  }

  std::optional<int> RenderRasterizerOptions::msaaSamples() const {
    return m_msaaSamples;
  }

  std::optional<std::string> RenderRasterizerOptions::msaaShadingMode() const {
    return m_msaaShadingMode;
  }

  std::optional<int> RenderRasterizerOptions::shadowMapSize() const {
    return m_shadowMapSize;
  }

  std::optional<int> RenderRasterizerOptions::shadowCascadeCount() const {
    return m_shadowCascadeCount;
  }

  std::optional<double> RenderRasterizerOptions::shadowCascadeSplitLambda() const {
    return m_shadowCascadeSplitLambda;
  }

  std::optional<double> RenderRasterizerOptions::shadowBias() const {
    return m_shadowBias;
  }

  std::optional<double> RenderRasterizerOptions::shadowSlopeBias() const {
    return m_shadowSlopeBias;
  }

  std::optional<int> RenderRasterizerOptions::shadowFilterRadius() const {
    return m_shadowFilterRadius;
  }

  std::optional<std::string> RenderRasterizerOptions::shadowFilterMode() const {
    return m_shadowFilterMode;
  }

  std::optional<RenderRasterShadowMode> RenderRasterizerOptions::shadowMode() const {
    return m_shadowMode;
  }

  bool RenderWireframeOptions::empty() const {
    return !m_lod;
  }

  QJsonObject RenderWireframeOptions::toJson() const {
    QJsonObject object;
    if (m_lod)
      object["lod"] = *m_lod;
    return object;
  }

  RenderWireframeOptions RenderWireframeOptions::fromJson(const QJsonObject& object,
                                                          const std::string& path) {
    detail::rejectUnknownFields(object, path, {"lod"}, optionsError);
    RenderWireframeOptions options;
    if (detail::hasField(object, "lod"))
      options.setLod(detail::intField(object, "lod", path, optionsError));
    return options;
  }

  RenderWireframeOptions
  RenderWireframeOptions::mergedWith(const RenderWireframeOptions& overrides) const {
    RenderWireframeOptions result = *this;
    result.m_lod = overrideOptional(result.m_lod, overrides.m_lod);
    return result;
  }

  WireframePassState RenderWireframeOptions::passState() const {
    WireframePassState state;
    if (m_lod)
      state.setLod(*m_lod);
    return state;
  }

  void RenderWireframeOptions::setLod(int lod) {
    m_lod = std::max(0, lod);
  }

  std::optional<int> RenderWireframeOptions::lod() const {
    return m_lod;
  }

  bool RenderEngineOptions::empty() const {
    return m_raytracer.empty() && m_rasterizer.empty() && m_wireframe.empty();
  }

  QJsonObject RenderEngineOptions::toJson() const {
    QJsonObject object;
    if (!m_raytracer.empty())
      object["raytracer"] = m_raytracer.toJson();
    if (!m_rasterizer.empty())
      object["rasterizer"] = m_rasterizer.toJson();
    if (!m_wireframe.empty())
      object["wireframe"] = m_wireframe.toJson();
    return object;
  }

  RenderEngineOptions RenderEngineOptions::fromJson(const QJsonObject& object,
                                                    const std::string& path) {
    detail::rejectUnknownFields(object, path, {"raytracer", "rasterizer", "wireframe"}, optionsError);
    RenderEngineOptions options;
    const QJsonObject raytracer = detail::objectField(object, "raytracer", path, optionsError);
    if (!raytracer.isEmpty())
      options.m_raytracer = RenderRaytracerOptions::fromJson(raytracer, path + ".raytracer");
    const QJsonObject rasterizer = detail::objectField(object, "rasterizer", path, optionsError);
    if (!rasterizer.isEmpty())
      options.m_rasterizer = RenderRasterizerOptions::fromJson(rasterizer, path + ".rasterizer");
    const QJsonObject wireframe = detail::objectField(object, "wireframe", path, optionsError);
    if (!wireframe.isEmpty())
      options.m_wireframe = RenderWireframeOptions::fromJson(wireframe, path + ".wireframe");
    return options;
  }

  RenderEngineOptions RenderEngineOptions::mergedWith(const RenderEngineOptions& overrides) const {
    RenderEngineOptions result;
    result.m_raytracer = m_raytracer.mergedWith(overrides.m_raytracer);
    result.m_rasterizer = m_rasterizer.mergedWith(overrides.m_rasterizer);
    result.m_wireframe = m_wireframe.mergedWith(overrides.m_wireframe);
    return result;
  }

  RenderRaytracerOptions& RenderEngineOptions::raytracer() {
    return m_raytracer;
  }

  RenderRasterizerOptions& RenderEngineOptions::rasterizer() {
    return m_rasterizer;
  }

  RenderWireframeOptions& RenderEngineOptions::wireframe() {
    return m_wireframe;
  }

  const RenderRaytracerOptions& RenderEngineOptions::raytracer() const {
    return m_raytracer;
  }

  const RenderRasterizerOptions& RenderEngineOptions::rasterizer() const {
    return m_rasterizer;
  }

  const RenderWireframeOptions& RenderEngineOptions::wireframe() const {
    return m_wireframe;
  }
}
