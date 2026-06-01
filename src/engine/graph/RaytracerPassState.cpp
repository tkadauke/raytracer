#include "engine/graph/RaytracerPassState.h"

#include "engine/graph/RenderGraphTypes.h"
#include "engine/graph/RenderPlan.h"
#include "engine/raytracer/Raytracer.h"
#include "engine/wavefront/WavefrontRaytracer.h"
#include "render/PathTracingIntegrator.h"
#include "render/WhittedIntegrator.h"
#include "render/cameras/Camera.h"
#include "render/samplers/Sampler.h"
#include "render/samplers/SamplerFactory.h"
#include "render/viewplanes/ViewPlane.h"
#include "render/viewplanes/ViewPlaneFactory.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <stdexcept>
#include <utility>

namespace engine::graph {
  namespace {
    [[noreturn]] void stateError(const std::string& path, const std::string& message) {
      throw std::runtime_error("Invalid raytracer pass state at " + path + ": " + message);
    }

    bool hasField(const QJsonObject& object, const char* key) {
      return !object.value(key).isUndefined();
    }

    void rejectUnknownFields(const QJsonObject& object, const std::string& path,
                             std::initializer_list<const char*> allowed) {
      for (auto it = object.begin(); it != object.end(); ++it) {
        const std::string key = it.key().toStdString();
        const bool matched = std::find(allowed.begin(), allowed.end(), key) != allowed.end();
        if (!matched)
          stateError(path + "." + key, "unknown field");
      }
    }

    QJsonObject objectField(const QJsonObject& object, const char* key, const std::string& path) {
      const auto value = object.value(key);
      if (value.isUndefined())
        return {};
      if (!value.isObject())
        stateError(path + "." + key, "expected object");
      return value.toObject();
    }

    int intField(const QJsonObject& object, const char* key, const std::string& path) {
      const auto value = object.value(key);
      if (!value.isDouble())
        stateError(path + "." + key, "expected integer");

      const double number = value.toDouble();
      if (!std::isfinite(number) || std::floor(number) != number)
        stateError(path + "." + key, "expected integer");
      return static_cast<int>(number);
    }

    double doubleField(const QJsonObject& object, const char* key, const std::string& path) {
      const auto value = object.value(key);
      if (!value.isDouble())
        stateError(path + "." + key, "expected number");

      const double number = value.toDouble();
      if (!std::isfinite(number))
        stateError(path + "." + key, "expected finite number");
      return number;
    }

    bool boolField(const QJsonObject& object, const char* key, const std::string& path) {
      const auto value = object.value(key);
      if (!value.isBool())
        stateError(path + "." + key, "expected boolean");
      return value.toBool();
    }

    std::string stringField(const QJsonObject& object, const char* key, const std::string& path) {
      const auto value = object.value(key);
      if (!value.isString())
        stateError(path + "." + key, "expected string");
      return value.toString().toStdString();
    }

    QString qstr(const std::string& value) {
      return QString::fromStdString(value);
    }

    std::string samplerFactoryId(const std::string& name) {
      return name.size() >= 7 && name.substr(name.size() - 7) == "Sampler" ? name
                                                                           : name + "Sampler";
    }
  }

  RaytracerBeautyPassState RaytracerBeautyPassState::fromJson(const QJsonObject& object,
                                                              const std::string& path) {
    rejectUnknownFields(object, path, {"execution", "sampling", "viewPlane", "convergence"});

    RaytracerBeautyPassState state;
    const QJsonObject execution = objectField(object, "execution", path);
    rejectUnknownFields(execution, path + ".execution",
                        {"maxRecursionDepth", "threads", "queueSize", "integrator"});
    if (hasField(execution, "maxRecursionDepth"))
      state.setMaximumRecursionDepth(intField(execution, "maxRecursionDepth", path + ".execution"));
    if (hasField(execution, "threads"))
      state.setMaximumThreads(intField(execution, "threads", path + ".execution"));
    if (hasField(execution, "queueSize"))
      state.setQueueSize(intField(execution, "queueSize", path + ".execution"));
    if (hasField(execution, "integrator"))
      state.setIntegrator(stringField(execution, "integrator", path + ".execution"));

    const QJsonObject sampling = objectField(object, "sampling", path);
    rejectUnknownFields(sampling, path + ".sampling", {"sampler", "samplesPerPixel"});
    if (hasField(sampling, "sampler"))
      state.setSampler(stringField(sampling, "sampler", path + ".sampling"));
    if (hasField(sampling, "samplesPerPixel"))
      state.setSamplesPerPixel(intField(sampling, "samplesPerPixel", path + ".sampling"));

    const QJsonObject viewPlane = objectField(object, "viewPlane", path);
    rejectUnknownFields(viewPlane, path + ".viewPlane", {"type"});
    if (hasField(viewPlane, "type"))
      state.setViewPlane(stringField(viewPlane, "type", path + ".viewPlane"));

    const QJsonObject convergence = objectField(object, "convergence", path);
    rejectUnknownFields(convergence, path + ".convergence",
                        {"enabled", "activeSampleFractionThreshold", "radianceDeltaRmsThreshold"});
    if (hasField(convergence, "enabled"))
      state.setConvergenceEnabled(boolField(convergence, "enabled", path + ".convergence"));
    if (hasField(convergence, "activeSampleFractionThreshold")) {
      state.setConvergenceActiveSampleFractionThreshold(
        doubleField(convergence, "activeSampleFractionThreshold", path + ".convergence"));
    }
    if (hasField(convergence, "radianceDeltaRmsThreshold")) {
      state.setConvergenceRadianceDeltaRmsThreshold(
        doubleField(convergence, "radianceDeltaRmsThreshold", path + ".convergence"));
    }

    return state;
  }

  const RaytracerBeautyPassState* RaytracerBeautyPassState::fromPass(const RenderPassNode& pass) {
    if (pass.executor != RenderExecutorKind::Raytracer &&
        pass.executor != RenderExecutorKind::Wavefront)
      return nullptr;
    return pass.state ? pass.state->asRaytracerBeautyPassState() : nullptr;
  }

  RaytracerBeautyPassState RaytracerBeautyPassState::valueFromPass(const RenderPassNode& pass) {
    const auto* state = fromPass(pass);
    return state ? *state : RaytracerBeautyPassState();
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
    if (!execution.isEmpty())
      object["execution"] = execution;

    QJsonObject sampling;
    if (m_sampler)
      sampling["sampler"] = qstr(*m_sampler);
    if (m_samplesPerPixel)
      sampling["samplesPerPixel"] = *m_samplesPerPixel;
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

    return object;
  }

  bool RaytracerBeautyPassState::empty() const {
    return toJson().isEmpty();
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
    if (m_convergenceEnabled)
      wavefront.setConvergenceEnabled(*m_convergenceEnabled);
    if (m_convergenceActiveSampleFractionThreshold) {
      wavefront.setConvergenceActiveSampleFractionThreshold(
        *m_convergenceActiveSampleFractionThreshold);
    }
    if (m_convergenceRadianceDeltaRmsThreshold)
      wavefront.setConvergenceRadianceDeltaRmsThreshold(*m_convergenceRadianceDeltaRmsThreshold);

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
    m_maximumRecursionDepth = std::max(1, depth);
  }

  void RaytracerBeautyPassState::setMaximumThreads(int threads) {
    m_maximumThreads = std::max(1, threads);
  }

  void RaytracerBeautyPassState::setQueueSize(int queueSize) {
    m_queueSize = std::max(1, queueSize);
  }

  void RaytracerBeautyPassState::setIntegrator(std::string integrator) {
    m_integrator =
      normalizedIntegratorName(std::move(integrator), "parameters.execution.integrator");
  }

  void RaytracerBeautyPassState::setSampler(std::string sampler) {
    m_sampler = std::move(sampler);
  }

  void RaytracerBeautyPassState::setSamplesPerPixel(int samples) {
    m_samplesPerPixel = std::max(1, samples);
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
    m_convergenceRadianceDeltaRmsThreshold = std::max(0.0, threshold);
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

  std::optional<std::string> RaytracerBeautyPassState::sampler() const {
    return m_sampler;
  }

  std::optional<int> RaytracerBeautyPassState::samplesPerPixel() const {
    return m_samplesPerPixel;
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

  std::string RaytracerBeautyPassState::normalizedIntegratorName(std::string integrator,
                                                                 const std::string& path) {
    std::transform(integrator.begin(), integrator.end(), integrator.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    std::replace(integrator.begin(), integrator.end(), '-', '_');
    if (integrator == "whitted")
      return "whitted";
    if (integrator == "pathtracer" || integrator == "path_tracer" || integrator == "pt")
      return "pathtracer";
    stateError(path, "expected whitted or pathtracer");
  }

  std::unique_ptr<render::Integrator> RaytracerBeautyPassState::createIntegratorForPass() const {
    if (!m_integrator)
      return nullptr;
    if (*m_integrator == "pathtracer") {
      auto integrator = std::make_unique<render::PathTracingIntegrator>();
      if (m_maximumRecursionDepth)
        integrator->setMaximumRecursionDepth(*m_maximumRecursionDepth);
      return integrator;
    }
    return std::make_unique<render::WhittedIntegrator>();
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
    sampler->setup(m_samplesPerPixel.value_or(1), 83);
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
