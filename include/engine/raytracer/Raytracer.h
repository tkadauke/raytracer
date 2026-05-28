#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"
#include "core/math/Ray.h"
#include "core/math/Rect.h"

#include "render/RenderEngine.h"
#include "render/RayCaster.h"

#include <list>
#include <memory>

template<class T>
class Buffer;

namespace render {
  class Camera;
  class WhittedIntegrator;
  class Primitive;
  class Scene;
  class State;
}

namespace engine::raytracer {

  /**
    * @brief Whitted-style recursive raytracer — the historical (and
    *        currently only) `RenderEngine` implementation.
    *
    * Concrete subclass of `RenderEngine`. Adds:
    *
    *  - **The threading + tile dispatch loop** (`render(Buffer<Colord>&)`
    *    override). Tiles the image, dispatches each tile to a
    *    `QThreadPool` worker, blocks for completion. The orchestration
    *    lives in a pimpl so this header stays free of `<QtCore>`.
    *  - **Single-ray probes** (`primitiveForRay`, `rayState`,
    *    `rayColor`). Bypass the threading machinery — used by the
    *    interactive picking path in `Modeler` (mouse click → "what
    *    primitive is under the cursor?") and by tests pinning shading
    *    behaviour. `rayColor` is also the `RayCaster` compatibility
    *    callback that cameras and recursive materials call today; it
    *    delegates the single-ray radiance policy to
    *    `render::WhittedIntegrator`.
    *  - **Recursion-depth limit.** Specific to ray-recursive engines
    *    (raytracer, future path tracer). Wireframe / raster engines
    *    have no analogue, so it doesn't live on `RenderEngine`.
    *
    * Camera, scene, tonemap, and cancellation hooks all live on the
    * base class — see `RenderEngine` for those.
    *
    * @code
    * auto scene = std::make_shared<Scene>(Colord::black());
    * scene->add(...);
    * auto raytracer = std::make_shared<Raytracer>(scene);
    * raytracer->camera()->setPosition({0, 0, -5});
    *
    * Buffer<unsigned int> buffer(width, height);
    * raytracer->render(buffer);
    * @endcode
    *
    * @see RenderEngine — the abstract base.
    * @see render::WhittedIntegrator — the default single-ray radiance policy.
    * @see Camera, Scene, Tonemap.
    * @see render::State — per-ray state threaded through `rayColor`.
    */
  class Raytracer : public render::RenderEngine, public render::RayCaster {
  public:
    /**
      * Construct with a scene and a default `PinholeCamera` looking
      * at the origin from `(0, 0, -5)`. Useful for the common case
      * of "give me a basic render at sensible defaults"; for full
      * control over the camera type/position, use the two-argument
      * constructor.
      */
    explicit Raytracer(std::shared_ptr<render::Scene> scene);

    /**
      * Construct with a caller-supplied camera and scene. The
      * camera's view plane is left as whatever the camera's
      * constructor set it to; callers who care about pixel size /
      * sampler / interlacing should configure it themselves.
      */
    explicit Raytracer(std::shared_ptr<render::Camera> camera,
                       std::shared_ptr<render::Scene> scene);

    virtual ~Raytracer();

    using RenderEngine::render;
    std::shared_ptr<render::RenderEngine> cloneForRender() const override;

    /**
      * Tile-and-thread render into the HDR accumulator. Implements
      * the abstract `RenderEngine::render(Buffer<Colord>&)` virtual.
      */
    virtual void render(Buffer<Colord>& buffer) override;

    /**
      * Tile-and-thread render with inline tonemapping into a
      * packed-RGB display buffer. Each tile worker tonemaps its
      * pixels and writes packed `unsigned int` values as it goes —
      * so an interactive widget polling the buffer sees partial
      * output progressively. The HDR overload above doesn't have
      * this property (workers write `Colord` and the post-render
      * tonemap pass only runs after every tile is done), which is
      * why this override exists.
      */
    virtual void render(Buffer<unsigned int>& buffer) override;

    /**
      * Tile-and-thread render that writes the same ray-traced pixels to both an
      * HDR accumulator and a packed-RGB display buffer. Render-graph preview
      * uses this when downstream color passes still need HDR input, but the UI
      * should continue seeing progressive raytracer output while tiles finish.
      */
    void render(Buffer<Colord>& hdrBuffer, Buffer<unsigned int>& displayBuffer,
                std::shared_ptr<render::Tonemap> displayTonemap);

    /**
      * Single-ray geometry probe. Returns the `Primitive*` the ray
      * hits first, or `nullptr` if the ray misses everything. Does
      * not shade the hit, so it's cheap enough for interactive
      * picking from a mouse click.
      *
      * Pointer ownership stays with the scene; callers must not
      * delete or store across scene changes.
      */
    const render::Primitive* primitiveForRay(const Rayd& ray) const;

    /**
      * Single-ray probe that returns a populated `State` with the
      * `hitPoint` and recursion counters as if the ray were the
      * primary in a fresh render.
      */
    render::State rayState(const Rayd& ray) const;

    /**
      * Recursive shading entry point. Returns the colour produced by
      * tracing `ray`, performing material evaluation and recursive
      * reflection / transmission as needed. Mutates `state` —
      * recursion depth, hit-point, and (when `traceEvents` is on) the
      * event log are updated in place by the configured
      * `render::WhittedIntegrator`.
      *
      * Bottoms out at `setMaximumRecursionDepth(N)` returning the
      * scene background; misses also return the scene background; a
      * hit on a primitive with no material returns black.
      */
    Colord rayColor(const Rayd& ray, render::State& state) const override;

    /**
      * Request cancellation of an in-flight render. Tiles already
      * running complete before the call returns, but no new tiles
      * are scheduled. Idempotent.
      */
    virtual void cancel() override;

    /// Resets the cancellation flag. Call before a fresh render on
    /// a previously cancelled engine.
    virtual void uncancel() override;

    /**
      * @returns the framebuffer tiles currently being rendered by
      * worker threads. Used by the GUI's progress overlay; consumers
      * must not assume the list is stable across calls.
      */
    virtual std::list<Recti> activeTiles() const override;

    /**
      * @returns framebuffer tiles whose workers have completed. The GUI
      * copies these from the render thread's back buffer into its
      * immutable front image for progressive display.
      */
    virtual std::list<Recti> completedTiles() const override;

    /**
      * Sets the maximum number of recursive `rayColor` calls along
      * any single primary-ray path. When the limit is hit the
      * recursion bottoms out at the scene background — see
      * `rayColor` for why background and not black.
      *
      * The default is 10, chosen to handle glass-torus scenes
      * (4 surface crossings × reflection branches per hit) without
      * truncating visible energy.
      */
    void setMaximumRecursionDepth(int depth);

    /**
      * Sets the worker-thread count. Defaults to
      * `QThread::idealThreadCount()`; lower for predictable
      * benchmarking, higher won't help once you saturate cores.
      */
    void setMaximumThreads(int threads);

    /**
      * Sets the queue size used by the internal `QThreadPool`. The
      * default matches `idealThreadCount()`; raise it for many
      * small tiles, lower it to bound memory use during large
      * renders.
      */
    void setQueueSize(int queue);

    /**
      * Toggles the in-progress red overlay drawn over tiles that
      * are still rendering. Useful for interactive previews; turn
      * off for headless renders where the intermediate state isn't
      * visible anyway.
      */
    void setShowProgressIndicators(bool show);

  private:
    struct Private;
    std::unique_ptr<Private> p;
  };
}
