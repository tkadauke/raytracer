#pragma once

#include "engine/graph/RenderGraphCompiler.h"
#include "render/RenderEngine.h"

#include <memory>

namespace engine::graph {
  class RenderGraphExecutionObserver;

  /**
    * RenderEngine facade that renders through a compiled render graph.
    *
    * The current implementation executes the first supported graph slice: an
    * enabled whole-frame beauty pass using the existing raytracer, rasterizer,
    * or wireframe engine, optional color overlays such as the first wireframe
    * overlay pass, followed by simple color postprocess passes such as tonemap.
    * Passes execute serially in dependency order. It preserves the graph-facing
    * workflow: callers can compile a plan, inspect or override it, then execute
    * that plan.
    */
  class GraphRenderEngine : public render::RenderEngine {
  public:
    explicit GraphRenderEngine(std::shared_ptr<render::Scene> scene);
    GraphRenderEngine(std::shared_ptr<render::Camera> camera, std::shared_ptr<render::Scene> scene);
    ~GraphRenderEngine() override;

    using RenderEngine::render;
    std::shared_ptr<render::RenderEngine> cloneForRender() const override;

    /**
      * Replaces the high-level render intent used when no explicit plan is set.
      */
    void setIntent(RenderIntent intent);

    /**
      * @returns the high-level render intent used for automatic compilation.
      */
    const RenderIntent& intent() const;

    /**
      * Installs a caller-compiled plan. Rendering uses this plan until
      * `clearPlan()` is called.
      */
    void setPlan(RenderPlan plan);

    /**
      * Removes the explicit plan so the next render compiles from intent.
      */
    void clearPlan();

    /**
      * @returns true when rendering uses a caller-provided plan.
      */
    bool hasExplicitPlan() const;

    /**
      * @returns the caller-provided plan when one is installed, or `nullptr`
      * when rendering will compile from `intent()`.
      */
    const RenderPlan* explicitPlan() const;

    /**
      * Compiles a plan for @p target from the current render intent.
      */
    RenderPlan compilePlan(const RenderTargetSpec& target) const;

    /**
      * @returns the most recently compiled or executed plan.
      */
    const RenderPlan& lastPlan() const;

    /**
      * Installs an observer for live pass execution events.
      *
      * The observer is copied into `cloneForRender()` snapshots so UI callers
      * can watch the render worker execute the cloned graph engine.
      */
    void setExecutionObserver(std::shared_ptr<RenderGraphExecutionObserver> observer);

    /**
      * @returns the currently installed execution observer, if any.
      */
    std::shared_ptr<RenderGraphExecutionObserver> executionObserver() const;

    /**
      * Executes the current plan into @p buffer.
      *
      * This first graph execution slice supports enabled beauty passes,
      * wireframe overlays, and tonemap postprocess passes, with disabled-pass
      * default substitution and color passthrough. Unsupported pass shapes throw
      * `std::runtime_error` with the validation or execution reason.
      */
    void render(Buffer<Colord>& buffer) override;

    /**
      * Executes the current plan and packs the exported color resource directly
      * to display RGB.
      *
      * Unlike the base `RenderEngine` implementation, this does not run the
      * engine's tonemap a second time. The graph's tonemap node is the display
      * transform; disabling that node should expose the pre-tonemap graph
      * resource before ordinary 8-bit packing clamps it.
      */
    void render(Buffer<unsigned int>& buffer) override;

    void cancel() override;
    void uncancel() override;
    std::list<Recti> activeTiles() const override;
    std::list<Recti> completedTiles() const override;

  private:
    struct Private;
    std::unique_ptr<Private> p;
  };
}
