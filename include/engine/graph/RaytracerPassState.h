#pragma once

#include "engine/graph/RenderPassState.h"

#include <QJsonObject>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

namespace engine::raytracer {
  class Raytracer;
}

namespace render {
  class Camera;
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

    static RaytracerBeautyPassState fromJson(const QJsonObject& object,
                                             const std::string& path = "parameters");
    static const RaytracerBeautyPassState* fromPass(const RenderPassNode& pass);
    static RaytracerBeautyPassState valueFromPass(const RenderPassNode& pass);

    const RaytracerBeautyPassState* asRaytracerBeautyPassState() const override;
    QJsonObject toJson() const override;
    bool empty() const;
    void applyTo(Raytracer& raytracer) const;

    void writeTo(RenderPassNode& pass) const;
    std::size_t writeToRaytracerBeautyPasses(RenderPlan& plan) const;

    void setMaximumRecursionDepth(int depth);
    void setMaximumThreads(int threads);
    void setQueueSize(int queueSize);
    void setIntegrator(std::string integrator);
    void setSampler(std::string sampler);
    void setSamplesPerPixel(int samples);
    void setViewPlane(std::string viewPlane);

    std::optional<int> maximumRecursionDepth() const;
    std::optional<int> maximumThreads() const;
    std::optional<int> queueSize() const;
    std::optional<std::string> integrator() const;
    std::optional<std::string> sampler() const;
    std::optional<int> samplesPerPixel() const;
    std::optional<std::string> viewPlane() const;

  private:
    static std::string normalizedIntegratorName(std::string integrator, const std::string& path);
    [[nodiscard]] std::unique_ptr<render::Integrator> createIntegratorForPass() const;
    std::shared_ptr<render::Sampler> createSamplerForPass() const;
    std::shared_ptr<render::ViewPlane>
    createViewPlaneForPass(const std::shared_ptr<render::Camera>& camera) const;

    std::optional<int> m_maximumRecursionDepth;
    std::optional<int> m_maximumThreads;
    std::optional<int> m_queueSize;
    std::optional<std::string> m_integrator;
    std::optional<std::string> m_sampler;
    std::optional<int> m_samplesPerPixel;
    std::optional<std::string> m_viewPlane;
  };
}
