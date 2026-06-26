#pragma once

#include "engine/graph/RenderPassState.h"
#include "engine/graph/TracingExecutionPreference.h"
#include "render/WavefrontIntersectionBackend.h"

#include <QJsonObject>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace engine::raytracer {
  class Raytracer;
}

namespace engine::wavefront {
  class WavefrontRaytracer;
}

namespace render {
  class Camera;
  class Denoiser;
  class Integrator;
  class Sampler;
  class ViewPlane;
}

namespace engine::graph {
  class RenderPlan;
  struct RenderPassNode;

  /**
    * Typed state for built-in raytracer beauty graph passes.
    */
  class RaytracerBeautyPassState : public RenderPassState {
  public:
    using Raytracer = engine::raytracer::Raytracer;
    using WavefrontRaytracer = engine::wavefront::WavefrontRaytracer;

    static RaytracerBeautyPassState fromJson(const QJsonObject& object,
                                             const std::string& path = "parameters");
    static const RaytracerBeautyPassState* fromPass(const RenderPassNode& pass);
    static RaytracerBeautyPassState valueFromPass(const RenderPassNode& pass);

    const RaytracerBeautyPassState* asRaytracerBeautyPassState() const override;
    QJsonObject toJson() const override;
    bool empty() const;
    std::optional<std::string> compiledDiffusePathLoopFallbackReason() const;
    void applyTo(Raytracer& raytracer) const;
    void applyTo(WavefrontRaytracer& wavefront) const;

    void writeTo(RenderPassNode& pass) const;
    std::size_t writeToRaytracerBeautyPasses(RenderPlan& plan) const;

    void setMaximumRecursionDepth(int depth);
    void setMaximumThreads(int threads);
    void setQueueSize(int queueSize);
    void setIntegrator(std::string integrator);
    void setTracingBackend(std::string backend);
    void setTracingBackend(render::WavefrontIntersectionBackendChoice backend);
    void setTracingExecution(TracingExecutionPreference preference);
    void setTracingExecution(std::string preference);
    void setPredictedTracingExecution(TracingExecutionPreference preference);
    void setPredictedTracingExecution(std::string preference);
    void setTracingExecutionFallbackReason(std::string reason);
    void setIntersectionBackend(std::string backend);
    void setIntersectionBackend(render::WavefrontIntersectionBackendChoice backend);
    void setRussianRouletteDepth(int depth);
    void setDirectLightSamples(int samples);
    void setSampler(std::string sampler);
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
    std::optional<TracingExecutionPreference> predictedTracingExecution() const;
    const std::string& tracingExecutionFallbackReason() const;
    std::optional<render::WavefrontIntersectionBackendChoice> intersectionBackend() const;
    std::optional<int> russianRouletteDepth() const;
    std::optional<int> directLightSamples() const;
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
    static std::string normalizedIntegratorName(std::string integrator, const std::string& path);
    static std::string normalizedDenoiserName(std::string denoiser, const std::string& path);
    [[nodiscard]] std::unique_ptr<render::Integrator> createIntegratorForPass() const;
    [[nodiscard]] std::unique_ptr<render::Denoiser> createDenoiserForPass() const;
    std::shared_ptr<render::Sampler> createSamplerForPass() const;
    std::shared_ptr<render::ViewPlane>
    createViewPlaneForPass(const std::shared_ptr<render::Camera>& camera) const;

    std::optional<int> m_maximumRecursionDepth;
    std::optional<int> m_maximumThreads;
    std::optional<int> m_queueSize;
    std::optional<std::string> m_integrator;
    std::optional<render::WavefrontIntersectionBackendChoice> m_tracingBackend;
    std::optional<TracingExecutionPreference> m_tracingExecution;
    std::optional<TracingExecutionPreference> m_predictedTracingExecution;
    std::string m_tracingExecutionFallbackReason;
    std::optional<render::WavefrontIntersectionBackendChoice> m_intersectionBackend;
    std::optional<int> m_russianRouletteDepth;
    std::optional<int> m_directLightSamples;
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
}
