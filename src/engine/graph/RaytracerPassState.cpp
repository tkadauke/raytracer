#include "engine/graph/RaytracerPassState.h"

#include "engine/graph/RenderGraphTypes.h"
#include "engine/graph/detail/JsonStateHelpers.h"
#include "core/util/QStringUtil.h"
#include "core/util/StringUtil.h"
#include "engine/graph/RenderPlan.h"
#include "engine/raytracer/Raytracer.h"
#include "engine/wavefront/WavefrontRaytracer.h"
#include "render/PathTracingIntegrator.h"
#include "render/WhittedIntegrator.h"
#include "render/cameras/Camera.h"
#include "render/denoise/BilateralDenoiser.h"
#include "render/denoise/BoxDenoiser.h"
#include "render/denoise/Denoiser.h"
#include "render/samplers/Sampler.h"
#include "render/samplers/SamplerFactory.h"
#include "render/viewplanes/ViewPlane.h"
#include "render/viewplanes/ViewPlaneFactory.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <utility>

namespace engine::graph {
  namespace {
    [[noreturn]] void stateError(const std::string& path, const std::string& message) {
      detail::throwPassStateError("raytracer", path, message);
    }

    constexpr std::uint64_t maxExactJsonInteger = (1ull << 53) - 1;


    std::string samplerFactoryId(const std::string& name) {
      return name.size() >= 7 && name.substr(name.size() - 7) == "Sampler" ? name
                                                                           : name + "Sampler";
    }

    // Lowercases @p value and normalizes '-' separators to '_', the shared
    // vocabulary-matching step for the sample-stream-mode/integrator/denoiser
    // name parsers below.
    std::string normalizeToken(std::string value) {
      value = core::util::lowercase(std::move(value));
      std::replace(value.begin(), value.end(), '-', '_');
      return value;
    }

    std::string normalizedSampleStreamMode(std::string mode, const std::string& path) {
      mode = normalizeToken(std::move(mode));
      if (mode == "sampler" || mode == "sampler_backed")
        return "sampler";
      if (mode == "gpu_sample_stream" || mode == "gpu")
        return "gpu_sample_stream";
      stateError(path, "expected sampler or gpu_sample_stream");
    }
  }

  RaytracerBeautyPassState RaytracerBeautyPassState::fromJson(const QJsonObject& object,
                                                              const std::string& path) {
    detail::rejectUnknownFields(
      object, path,
      {"execution", "sampling", "viewPlane", "convergence", "adaptiveSampling", "denoise"}, stateError);

    RaytracerBeautyPassState state;
    const QJsonObject execution = detail::objectField(object, "execution", path, stateError);
    detail::rejectUnknownFields(execution, path + ".execution",
                        {"maxRecursionDepth", "threads", "queueSize", "integrator",
                         "tracingBackend", "tracingExecution", "predictedTracingExecution",
                         "tracingExecutionFallbackReason", "intersectionBackend",
                         "russianRouletteDepth", "directLightSamples",
                         "gpuPrimarySampleChunkSize"}, stateError);
    if (detail::hasField(execution, "maxRecursionDepth"))
      state.setMaximumRecursionDepth(detail::intField(execution, "maxRecursionDepth", path + ".execution", stateError));
    if (detail::hasField(execution, "threads"))
      state.setMaximumThreads(detail::intField(execution, "threads", path + ".execution", stateError));
    if (detail::hasField(execution, "queueSize"))
      state.setQueueSize(detail::intField(execution, "queueSize", path + ".execution", stateError));
    if (detail::hasField(execution, "integrator"))
      state.setIntegrator(detail::stringField(execution, "integrator", path + ".execution", stateError));
    if (detail::hasField(execution, "tracingBackend"))
      state.setTracingBackend(detail::stringField(execution, "tracingBackend", path + ".execution", stateError));
    if (detail::hasField(execution, "tracingExecution")) {
      state.setTracingExecution(detail::stringField(execution, "tracingExecution", path + ".execution", stateError));
    }
    if (detail::hasField(execution, "predictedTracingExecution")) {
      state.setPredictedTracingExecution(
        detail::stringField(execution, "predictedTracingExecution", path + ".execution", stateError));
    }
    if (detail::hasField(execution, "tracingExecutionFallbackReason")) {
      state.setTracingExecutionFallbackReason(
        detail::stringField(execution, "tracingExecutionFallbackReason", path + ".execution", stateError));
    }
    if (detail::hasField(execution, "intersectionBackend")) {
      state.setIntersectionBackend(
        detail::stringField(execution, "intersectionBackend", path + ".execution", stateError));
    }
    if (detail::hasField(execution, "russianRouletteDepth")) {
      state.setRussianRouletteDepth(
        detail::intField(execution, "russianRouletteDepth", path + ".execution", stateError));
    }
    if (detail::hasField(execution, "directLightSamples")) {
      state.setDirectLightSamples(detail::intField(execution, "directLightSamples", path + ".execution", stateError));
    }
    if (detail::hasField(execution, "gpuPrimarySampleChunkSize")) {
      state.setGpuPrimarySampleChunkSize(
        detail::intField(execution, "gpuPrimarySampleChunkSize", path + ".execution", stateError));
    }

    const QJsonObject sampling = detail::objectField(object, "sampling", path, stateError);
    detail::rejectUnknownFields(sampling, path + ".sampling",
                        {"sampler", "samplesPerPixel", "seed", "streamMode"}, stateError);
    if (detail::hasField(sampling, "sampler"))
      state.setSampler(detail::stringField(sampling, "sampler", path + ".sampling", stateError));
    if (detail::hasField(sampling, "samplesPerPixel"))
      state.setSamplesPerPixel(detail::intField(sampling, "samplesPerPixel", path + ".sampling", stateError));
    if (detail::hasField(sampling, "seed"))
      state.setSamplingSeed(detail::uint64Field(sampling, "seed", path + ".sampling", stateError));
    if (detail::hasField(sampling, "streamMode"))
      state.setSampleStreamMode(detail::stringField(sampling, "streamMode", path + ".sampling", stateError));

    const QJsonObject viewPlane = detail::objectField(object, "viewPlane", path, stateError);
    detail::rejectUnknownFields(viewPlane, path + ".viewPlane", {"type"}, stateError);
    if (detail::hasField(viewPlane, "type"))
      state.setViewPlane(detail::stringField(viewPlane, "type", path + ".viewPlane", stateError));

    const QJsonObject convergence = detail::objectField(object, "convergence", path, stateError);
    detail::rejectUnknownFields(convergence, path + ".convergence",
                        {"enabled", "activeSampleFractionThreshold", "radianceDeltaRmsThreshold"}, stateError);
    if (detail::hasField(convergence, "enabled"))
      state.setConvergenceEnabled(detail::boolField(convergence, "enabled", path + ".convergence", stateError));
    if (detail::hasField(convergence, "activeSampleFractionThreshold")) {
      state.setConvergenceActiveSampleFractionThreshold(
        detail::doubleField(convergence, "activeSampleFractionThreshold", path + ".convergence", stateError));
    }
    if (detail::hasField(convergence, "radianceDeltaRmsThreshold")) {
      state.setConvergenceRadianceDeltaRmsThreshold(
        detail::doubleField(convergence, "radianceDeltaRmsThreshold", path + ".convergence", stateError));
    }

    const QJsonObject adaptiveSampling = detail::objectField(object, "adaptiveSampling", path, stateError);
    detail::rejectUnknownFields(adaptiveSampling, path + ".adaptiveSampling",
                        {"enabled", "minimumSamples", "stddevThreshold"}, stateError);
    if (detail::hasField(adaptiveSampling, "enabled"))
      state.setAdaptiveSamplingEnabled(
        detail::boolField(adaptiveSampling, "enabled", path + ".adaptiveSampling", stateError));
    if (detail::hasField(adaptiveSampling, "minimumSamples"))
      state.setAdaptiveMinimumSamples(
        detail::intField(adaptiveSampling, "minimumSamples", path + ".adaptiveSampling", stateError));
    if (detail::hasField(adaptiveSampling, "stddevThreshold")) {
      state.setAdaptiveStddevThreshold(
        detail::doubleField(adaptiveSampling, "stddevThreshold", path + ".adaptiveSampling", stateError));
    }

    const QJsonObject denoise = detail::objectField(object, "denoise", path, stateError);
    detail::rejectUnknownFields(denoise, path + ".denoise", {"type", "radius", "colorSigma"}, stateError);
    if (detail::hasField(denoise, "type"))
      state.setDenoiser(detail::stringField(denoise, "type", path + ".denoise", stateError));
    if (detail::hasField(denoise, "radius"))
      state.setDenoiseRadius(detail::intField(denoise, "radius", path + ".denoise", stateError));
    if (detail::hasField(denoise, "colorSigma"))
      state.setDenoiseColorSigma(detail::doubleField(denoise, "colorSigma", path + ".denoise", stateError));

    return state;
  }

  const RaytracerBeautyPassState* RaytracerBeautyPassState::fromPass(const RenderPassNode& pass) {
    if (pass.executor != RenderExecutorKind::Raytracer &&
        pass.executor != RenderExecutorKind::Wavefront)
      return nullptr;
    return pass.state ? pass.state->asRaytracerBeautyPassState() : nullptr;
  }

  RaytracerBeautyPassState RaytracerBeautyPassState::valueFromPass(const RenderPassNode& pass) {
    return detail::valueOrDefault(fromPass(pass));
  }

  const RaytracerBeautyPassState* RaytracerBeautyPassState::asRaytracerBeautyPassState() const {
    return this;
  }

  QJsonObject RaytracerBeautyPassState::toJson() const {
    QJsonObject object;

    QJsonObject execution;
    if (m_maximumRecursionDepth)
      execution["maxRecursionDepth"] = *m_maximumRecursionDepth;
    if (m_maximumThreads)
      execution["threads"] = *m_maximumThreads;
    if (m_queueSize)
      execution["queueSize"] = *m_queueSize;
    if (m_integrator)
      execution["integrator"] = qstr(*m_integrator);
    if (m_tracingBackend)
      execution["tracingBackend"] = qstr(m_tracingBackend->id());
    if (m_tracingExecution)
      execution["tracingExecution"] = qstr(tracingExecutionPreferenceName(*m_tracingExecution));
    if (m_predictedTracingExecution)
      execution["predictedTracingExecution"] =
        qstr(tracingExecutionPreferenceName(*m_predictedTracingExecution));
    if (!m_tracingExecutionFallbackReason.empty())
      execution["tracingExecutionFallbackReason"] = qstr(m_tracingExecutionFallbackReason);
    if (m_intersectionBackend)
      execution["intersectionBackend"] = qstr(m_intersectionBackend->id());
    if (m_russianRouletteDepth)
      execution["russianRouletteDepth"] = *m_russianRouletteDepth;
    if (m_directLightSamples)
      execution["directLightSamples"] = *m_directLightSamples;
    if (m_gpuPrimarySampleChunkSize)
      execution["gpuPrimarySampleChunkSize"] = *m_gpuPrimarySampleChunkSize;
    if (!execution.isEmpty())
      object["execution"] = execution;

    QJsonObject sampling;
    if (m_sampler)
      sampling["sampler"] = qstr(*m_sampler);
    if (m_samplesPerPixel)
      sampling["samplesPerPixel"] = *m_samplesPerPixel;
    if (m_samplingSeed)
      sampling["seed"] = static_cast<double>(*m_samplingSeed);
    if (m_sampleStreamMode || !sampling.isEmpty())
      sampling["streamMode"] = qstr(m_sampleStreamMode.value_or("sampler"));
    if (!sampling.isEmpty())
      object["sampling"] = sampling;

    QJsonObject viewPlane;
    if (m_viewPlane)
      viewPlane["type"] = qstr(*m_viewPlane);
    if (!viewPlane.isEmpty())
      object["viewPlane"] = viewPlane;

    QJsonObject convergence;
    if (m_convergenceEnabled)
      convergence["enabled"] = *m_convergenceEnabled;
    if (m_convergenceActiveSampleFractionThreshold) {
      convergence["activeSampleFractionThreshold"] = *m_convergenceActiveSampleFractionThreshold;
    }
    if (m_convergenceRadianceDeltaRmsThreshold)
      convergence["radianceDeltaRmsThreshold"] = *m_convergenceRadianceDeltaRmsThreshold;
    if (!convergence.isEmpty())
      object["convergence"] = convergence;

    QJsonObject adaptiveSampling;
    if (m_adaptiveSamplingEnabled)
      adaptiveSampling["enabled"] = *m_adaptiveSamplingEnabled;
    if (m_adaptiveMinimumSamples)
      adaptiveSampling["minimumSamples"] = *m_adaptiveMinimumSamples;
    if (m_adaptiveStddevThreshold)
      adaptiveSampling["stddevThreshold"] = *m_adaptiveStddevThreshold;
    if (!adaptiveSampling.isEmpty())
      object["adaptiveSampling"] = adaptiveSampling;

    QJsonObject denoise;
    if (m_denoiser) {
      denoise["type"] = qstr(*m_denoiser);
    } else if (m_denoiseColorSigma) {
      denoise["type"] = QStringLiteral("bilateral");
    } else if (m_denoiseRadius) {
      denoise["type"] = QStringLiteral("box");
    }
    if (m_denoiseRadius && (!m_denoiser || *m_denoiser != "none"))
      denoise["radius"] = *m_denoiseRadius;
    const bool denoiseIsBilateral =
      (m_denoiser && *m_denoiser == "bilateral") || (!m_denoiser && m_denoiseColorSigma);
    if (m_denoiseColorSigma && denoiseIsBilateral)
      denoise["colorSigma"] = *m_denoiseColorSigma;
    if (!denoise.isEmpty())
      object["denoise"] = denoise;

    return object;
  }

  bool RaytracerBeautyPassState::empty() const {
    return toJson().isEmpty();
  }

  std::optional<std::string>
  RaytracerBeautyPassState::compiledDiffusePathLoopBackendFallbackReason() const {
    if (integrator().value_or("whitted") != "pathtracer") {
      return "compiled diffuse path loop currently supports only the pathtracer integrator";
    }
    if (adaptiveSamplingEnabled().value_or(false)) {
      return "compiled diffuse path loop does not support adaptive sampling yet";
    }
    return std::nullopt;
  }

  std::optional<std::string>
  RaytracerBeautyPassState::compiledDiffusePathLoopDevicePrimaryFallbackReason() const {
    if (sampleStreamMode() && *sampleStreamMode() != "gpu_sample_stream") {
      return "compiled diffuse path loop requires the GPU sample stream";
    }
    if (!sampleStreamMode() && sampler()) {
      return "compiled diffuse path loop requires the GPU sample stream";
    }
    return std::nullopt;
  }

  std::optional<std::string>
  RaytracerBeautyPassState::compiledDiffusePathLoopFallbackReason() const {
    if (const auto backendReason = compiledDiffusePathLoopBackendFallbackReason()) {
      return backendReason;
    }
    return compiledDiffusePathLoopDevicePrimaryFallbackReason();
  }

  void RaytracerBeautyPassState::applyTo(Raytracer& raytracer) const {
    if (auto integrator = createIntegratorForPass())
      raytracer.setIntegrator(std::move(integrator));
    if (m_maximumRecursionDepth)
      raytracer.setMaximumRecursionDepth(*m_maximumRecursionDepth);
    if (m_maximumThreads)
      raytracer.setMaximumThreads(*m_maximumThreads);
    if (m_queueSize)
      raytracer.setQueueSize(*m_queueSize);
    if (m_samplingSeed)
      raytracer.setSamplingSeed(*m_samplingSeed);

    auto viewPlane = createViewPlaneForPass(raytracer.camera());
    if (viewPlane && raytracer.camera())
      raytracer.camera()->setViewPlane(std::move(viewPlane));
  }

  void RaytracerBeautyPassState::applyTo(WavefrontRaytracer& wavefront) const {
    if (auto integrator = createIntegratorForPass())
      wavefront.setIntegrator(std::move(integrator));
    if (m_maximumRecursionDepth)
      wavefront.setMaximumRecursionDepth(*m_maximumRecursionDepth);
    if (m_maximumThreads)
      wavefront.setMaximumThreads(*m_maximumThreads);
    if (m_queueSize)
      wavefront.setQueueSize(*m_queueSize);
    if (m_samplingSeed)
      wavefront.setSamplingSeed(*m_samplingSeed);
    if (m_tracingBackend) {
      wavefront.setIntersectionBackend(*m_tracingBackend);
    } else if (m_intersectionBackend) {
      wavefront.setIntersectionBackend(*m_intersectionBackend);
    }
    if (m_convergenceEnabled)
      wavefront.setConvergenceEnabled(*m_convergenceEnabled);
    if (m_convergenceActiveSampleFractionThreshold) {
      wavefront.setConvergenceActiveSampleFractionThreshold(
        *m_convergenceActiveSampleFractionThreshold);
    }
    if (m_convergenceRadianceDeltaRmsThreshold)
      wavefront.setConvergenceRadianceDeltaRmsThreshold(*m_convergenceRadianceDeltaRmsThreshold);
    if (m_adaptiveSamplingEnabled)
      wavefront.setAdaptiveSamplingEnabled(*m_adaptiveSamplingEnabled);
    if (m_adaptiveMinimumSamples)
      wavefront.setAdaptiveMinimumSamples(*m_adaptiveMinimumSamples);
    if (m_adaptiveStddevThreshold)
      wavefront.setAdaptiveStddevThreshold(*m_adaptiveStddevThreshold);
    if (m_denoiser && *m_denoiser == "none") {
      wavefront.clearDenoiser();
    } else if (auto denoiser = createDenoiserForPass()) {
      wavefront.setDenoiser(std::move(denoiser));
    }

    auto viewPlane = createViewPlaneForPass(wavefront.camera());
    if (viewPlane && wavefront.camera())
      wavefront.camera()->setViewPlane(std::move(viewPlane));
  }

  void RaytracerBeautyPassState::writeTo(RenderPassNode& pass) const {
    if (empty()) {
      pass.state.reset();
    } else {
      pass.state = std::make_shared<RaytracerBeautyPassState>(*this);
    }
  }

  std::size_t RaytracerBeautyPassState::writeToRaytracerBeautyPasses(RenderPlan& plan) const {
    const std::shared_ptr<const RenderPassState> state =
      empty() ? nullptr : std::make_shared<RaytracerBeautyPassState>(*this);
    return plan.setPassState(RenderPassKind::Beauty, RenderExecutorKind::Raytracer, state) +
           plan.setPassState(RenderPassKind::Beauty, RenderExecutorKind::Wavefront, state);
  }

  void RaytracerBeautyPassState::setMaximumRecursionDepth(int depth) {
    m_maximumRecursionDepth = atLeast(1, depth);
  }

  void RaytracerBeautyPassState::setMaximumThreads(int threads) {
    m_maximumThreads = atLeast(1, threads);
  }

  void RaytracerBeautyPassState::setQueueSize(int queueSize) {
    m_queueSize = atLeast(1, queueSize);
  }

  void RaytracerBeautyPassState::setIntegrator(std::string integrator) {
    m_integrator =
      normalizedIntegratorName(std::move(integrator), "parameters.execution.integrator");
  }

  void RaytracerBeautyPassState::setTracingBackend(std::string backend) {
    try {
      m_tracingBackend = render::WavefrontIntersectionBackendChoice::fromString(std::move(backend));
    } catch (const std::invalid_argument&) {
      stateError("parameters.execution.tracingBackend", "expected auto, cpu, or gpu");
    }
  }

  void
  RaytracerBeautyPassState::setTracingBackend(render::WavefrontIntersectionBackendChoice backend) {
    m_tracingBackend = backend;
  }

  void RaytracerBeautyPassState::setTracingExecution(TracingExecutionPreference preference) {
    m_tracingExecution = preference;
  }

  void RaytracerBeautyPassState::setTracingExecution(std::string preference) {
    const auto parsed = tracingExecutionPreferenceFromString(preference);
    if (!parsed)
      stateError("parameters.execution.tracingExecution", "expected auto, cpu, hybrid, or gpu");
    m_tracingExecution = *parsed;
  }

  void
  RaytracerBeautyPassState::setPredictedTracingExecution(TracingExecutionPreference preference) {
    m_predictedTracingExecution = preference;
  }

  void RaytracerBeautyPassState::setPredictedTracingExecution(std::string preference) {
    const auto parsed = tracingExecutionPreferenceFromString(preference);
    if (!parsed) {
      stateError("parameters.execution.predictedTracingExecution",
                 "expected auto, cpu, hybrid, or gpu");
    }
    m_predictedTracingExecution = *parsed;
  }

  void RaytracerBeautyPassState::setTracingExecutionFallbackReason(std::string reason) {
    m_tracingExecutionFallbackReason = std::move(reason);
  }

  void RaytracerBeautyPassState::setIntersectionBackend(std::string backend) {
    try {
      m_intersectionBackend =
        render::WavefrontIntersectionBackendChoice::fromString(std::move(backend));
    } catch (const std::invalid_argument&) {
      stateError("parameters.execution.intersectionBackend", "expected auto, cpu, or gpu");
    }
  }

  void RaytracerBeautyPassState::setIntersectionBackend(
    render::WavefrontIntersectionBackendChoice backend) {
    m_intersectionBackend = backend;
  }

  void RaytracerBeautyPassState::setRussianRouletteDepth(int depth) {
    m_russianRouletteDepth = atLeast(1, depth);
  }

  void RaytracerBeautyPassState::setDirectLightSamples(int samples) {
    m_directLightSamples = atLeast(1, samples);
  }

  void RaytracerBeautyPassState::setGpuPrimarySampleChunkSize(int samples) {
    m_gpuPrimarySampleChunkSize = atLeast(0, samples);
  }

  void RaytracerBeautyPassState::setSampler(std::string sampler) {
    m_sampler = std::move(sampler);
  }

  void RaytracerBeautyPassState::setSamplesPerPixel(int samples) {
    m_samplesPerPixel = atLeast(1, samples);
  }

  void RaytracerBeautyPassState::setSamplingSeed(std::uint64_t seed) {
    if (seed > maxExactJsonInteger) {
      stateError("parameters.sampling.seed", "expected exactly representable JSON integer");
    }
    m_samplingSeed = seed;
  }

  void RaytracerBeautyPassState::setSampleStreamMode(std::string mode) {
    m_sampleStreamMode =
      normalizedSampleStreamMode(std::move(mode), "parameters.sampling.streamMode");
  }

  void RaytracerBeautyPassState::setViewPlane(std::string viewPlane) {
    m_viewPlane = std::move(viewPlane);
  }

  void RaytracerBeautyPassState::setConvergenceEnabled(bool enabled) {
    m_convergenceEnabled = enabled;
  }

  void RaytracerBeautyPassState::setConvergenceActiveSampleFractionThreshold(double fraction) {
    m_convergenceActiveSampleFractionThreshold = std::clamp(fraction, 0.0, 1.0);
  }

  void RaytracerBeautyPassState::setConvergenceRadianceDeltaRmsThreshold(double threshold) {
    m_convergenceRadianceDeltaRmsThreshold = atLeast(0.0, threshold);
  }

  void RaytracerBeautyPassState::setAdaptiveSamplingEnabled(bool enabled) {
    m_adaptiveSamplingEnabled = enabled;
  }

  void RaytracerBeautyPassState::setAdaptiveMinimumSamples(int samples) {
    m_adaptiveMinimumSamples = atLeast(1, samples);
  }

  void RaytracerBeautyPassState::setAdaptiveStddevThreshold(double threshold) {
    m_adaptiveStddevThreshold = atLeast(0.0, threshold);
  }

  void RaytracerBeautyPassState::setDenoiser(std::string denoiser) {
    m_denoiser = normalizedDenoiserName(std::move(denoiser), "parameters.denoise.type");
  }

  void RaytracerBeautyPassState::setDenoiseRadius(int radius) {
    m_denoiseRadius = atLeast(0, radius);
  }

  void RaytracerBeautyPassState::setDenoiseColorSigma(double sigma) {
    m_denoiseColorSigma = atLeast(0.0, sigma);
  }

  std::optional<int> RaytracerBeautyPassState::maximumRecursionDepth() const {
    return m_maximumRecursionDepth;
  }

  std::optional<int> RaytracerBeautyPassState::maximumThreads() const {
    return m_maximumThreads;
  }

  std::optional<int> RaytracerBeautyPassState::queueSize() const {
    return m_queueSize;
  }

  std::optional<std::string> RaytracerBeautyPassState::integrator() const {
    return m_integrator;
  }

  std::optional<render::WavefrontIntersectionBackendChoice>
  RaytracerBeautyPassState::tracingBackend() const {
    return m_tracingBackend;
  }

  std::optional<TracingExecutionPreference> RaytracerBeautyPassState::tracingExecution() const {
    return m_tracingExecution;
  }

  std::optional<TracingExecutionPreference>
  RaytracerBeautyPassState::predictedTracingExecution() const {
    return m_predictedTracingExecution;
  }

  const std::string& RaytracerBeautyPassState::tracingExecutionFallbackReason() const {
    return m_tracingExecutionFallbackReason;
  }

  std::optional<render::WavefrontIntersectionBackendChoice>
  RaytracerBeautyPassState::intersectionBackend() const {
    return m_intersectionBackend;
  }

  std::optional<int> RaytracerBeautyPassState::russianRouletteDepth() const {
    return m_russianRouletteDepth;
  }

  std::optional<int> RaytracerBeautyPassState::directLightSamples() const {
    return m_directLightSamples;
  }

  std::optional<int> RaytracerBeautyPassState::gpuPrimarySampleChunkSize() const {
    return m_gpuPrimarySampleChunkSize;
  }

  std::optional<std::string> RaytracerBeautyPassState::sampler() const {
    return m_sampler;
  }

  std::optional<int> RaytracerBeautyPassState::samplesPerPixel() const {
    return m_samplesPerPixel;
  }

  std::optional<std::uint64_t> RaytracerBeautyPassState::samplingSeed() const {
    return m_samplingSeed;
  }

  std::optional<std::string> RaytracerBeautyPassState::sampleStreamMode() const {
    return m_sampleStreamMode;
  }

  std::optional<std::string> RaytracerBeautyPassState::viewPlane() const {
    return m_viewPlane;
  }

  std::optional<bool> RaytracerBeautyPassState::convergenceEnabled() const {
    return m_convergenceEnabled;
  }

  std::optional<double> RaytracerBeautyPassState::convergenceActiveSampleFractionThreshold() const {
    return m_convergenceActiveSampleFractionThreshold;
  }

  std::optional<double> RaytracerBeautyPassState::convergenceRadianceDeltaRmsThreshold() const {
    return m_convergenceRadianceDeltaRmsThreshold;
  }

  std::optional<bool> RaytracerBeautyPassState::adaptiveSamplingEnabled() const {
    return m_adaptiveSamplingEnabled;
  }

  std::optional<int> RaytracerBeautyPassState::adaptiveMinimumSamples() const {
    return m_adaptiveMinimumSamples;
  }

  std::optional<double> RaytracerBeautyPassState::adaptiveStddevThreshold() const {
    return m_adaptiveStddevThreshold;
  }

  std::optional<std::string> RaytracerBeautyPassState::denoiser() const {
    return m_denoiser;
  }

  std::optional<int> RaytracerBeautyPassState::denoiseRadius() const {
    return m_denoiseRadius;
  }

  std::optional<double> RaytracerBeautyPassState::denoiseColorSigma() const {
    return m_denoiseColorSigma;
  }

  std::string RaytracerBeautyPassState::normalizedIntegratorName(std::string integrator,
                                                                 const std::string& path) {
    integrator = normalizeToken(std::move(integrator));
    if (integrator == "whitted")
      return "whitted";
    if (integrator == "pathtracer" || integrator == "path_tracer" || integrator == "pt")
      return "pathtracer";
    stateError(path, "expected whitted or pathtracer");
  }

  std::string RaytracerBeautyPassState::normalizedDenoiserName(std::string denoiser,
                                                               const std::string& path) {
    denoiser = normalizeToken(std::move(denoiser));
    if (denoiser == "none" || denoiser == "off" || denoiser == "disabled")
      return "none";
    if (denoiser == "box" || denoiser == "box_filter")
      return "box";
    if (denoiser == "bilateral" || denoiser == "bilateral_filter" || denoiser == "color_bilateral")
      return "bilateral";
    stateError(path, "expected none, box, or bilateral");
  }

  std::unique_ptr<render::Integrator> RaytracerBeautyPassState::createIntegratorForPass() const {
    if (!m_integrator)
      return nullptr;
    if (*m_integrator == "pathtracer") {
      auto integrator = std::make_unique<render::PathTracingIntegrator>();
      if (m_maximumRecursionDepth)
        integrator->setMaximumRecursionDepth(*m_maximumRecursionDepth);
      if (m_russianRouletteDepth)
        integrator->setRussianRouletteDepth(*m_russianRouletteDepth);
      if (m_directLightSamples)
        integrator->setDirectLightSamples(*m_directLightSamples);
      return integrator;
    }
    return std::make_unique<render::WhittedIntegrator>();
  }

  std::unique_ptr<render::Denoiser> RaytracerBeautyPassState::createDenoiserForPass() const {
    if (m_denoiser && *m_denoiser == "none")
      return nullptr;
    if ((m_denoiser && *m_denoiser == "bilateral") || (!m_denoiser && m_denoiseColorSigma)) {
      return std::make_unique<render::BilateralDenoiser>(m_denoiseRadius.value_or(2),
                                                         m_denoiseColorSigma.value_or(0.1));
    }
    if ((m_denoiser && *m_denoiser == "box") || (!m_denoiser && m_denoiseRadius))
      return std::make_unique<render::BoxDenoiser>(m_denoiseRadius.value_or(1));
    return nullptr;
  }

  std::shared_ptr<render::Sampler> RaytracerBeautyPassState::createSamplerForPass() const {
    if (!m_sampler && !m_samplesPerPixel)
      return nullptr;

    const std::string factoryId = samplerFactoryId(m_sampler.value_or("Regular"));
    auto sampler = render::SamplerFactory::self().createShared(factoryId);
    if (!sampler) {
      throw std::runtime_error("raytracer pass state requested unknown sampler '" + factoryId +
                               "'");
    }
    if (m_samplingSeed) {
      sampler->setup(m_samplesPerPixel.value_or(1), 83, *m_samplingSeed);
    } else {
      sampler->setup(m_samplesPerPixel.value_or(1), 83);
    }
    return sampler;
  }

  std::shared_ptr<render::ViewPlane> RaytracerBeautyPassState::createViewPlaneForPass(
    const std::shared_ptr<render::Camera>& camera) const {
    if (!m_viewPlane && !m_sampler && !m_samplesPerPixel)
      return nullptr;

    std::shared_ptr<render::ViewPlane> viewPlane;
    if (m_viewPlane) {
      viewPlane = render::ViewPlaneFactory::self().createShared(*m_viewPlane);
      if (!viewPlane) {
        throw std::runtime_error("raytracer pass state requested unknown view plane '" +
                                 *m_viewPlane + "'");
      }
    } else if (camera && camera->viewPlane()) {
      viewPlane = camera->viewPlane()->clone();
    } else {
      viewPlane = render::ViewPlaneFactory::self().createShared("ViewPlane");
    }

    if (!viewPlane) {
      throw std::runtime_error("raytracer pass state could not create a view plane");
    }

    if (auto sampler = createSamplerForPass())
      viewPlane->setSampler(std::move(sampler));

    return viewPlane;
  }
}
