#pragma once

#include "core/math/Rect.h"
#include "render/RenderEngine.h"

#include <cstdint>
#include <list>
#include <memory>
#include <optional>

template<class T>
class Buffer;

namespace render {
  class Camera;
  class Integrator;
  class Scene;
}

namespace engine::wavefront {

  /**
    * @brief Depth-major ray rendering engine scaffold.
    *
    * `WavefrontRaytracer` is a sibling to `engine::raytracer::Raytracer`,
    * not a replacement. It owns framebuffer/tile scheduling and progressive
    * display state like every `RenderEngine`, but deliberately does not expose
    * the public `RayCaster` probe API (`rayColor`, `rayState`,
    * `primitiveForRay`). Single-ray picking stays on the recursive raytracer.
    *
    * The first implementation reuses the existing camera/sample-stream
    * contract and a private recursive adapter for legacy Whitted material
    * callbacks. Follow-up work can move the tile body to explicit per-depth
    * queues without changing the graph, CLI, or UI executor surface.
    */
  class WavefrontRaytracer : public render::RenderEngine {
  public:
    explicit WavefrontRaytracer(std::shared_ptr<render::Scene> scene);
    explicit WavefrontRaytracer(std::shared_ptr<render::Camera> camera,
                                std::shared_ptr<render::Scene> scene);
    ~WavefrontRaytracer() override;

    using RenderEngine::render;
    std::shared_ptr<render::RenderEngine> cloneForRender() const override;

    void render(Buffer<Colord>& buffer) override;
    void render(Buffer<unsigned int>& buffer) override;
    void render(Buffer<Colord>& hdrBuffer, Buffer<unsigned int>& displayBuffer,
                std::shared_ptr<render::Tonemap> displayTonemap);

    void cancel() override;
    void uncancel() override;
    std::list<Recti> activeTiles() const override;
    std::list<Recti> completedTiles() const override;

    void setIntegrator(std::unique_ptr<render::Integrator> integrator);
    const render::Integrator& integrator() const;
    void setMaximumRecursionDepth(int depth);

    void setSamplingSeed(std::uint64_t seed);
    void clearSamplingSeed();
    std::optional<std::uint64_t> samplingSeed() const;

    void setMaximumThreads(int threads);
    void setQueueSize(int queue);
    void setShowProgressIndicators(bool show);

  private:
    struct Private;
    std::unique_ptr<Private> p;
  };
}
