#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"
#include "core/math/Ray.h"
#include "core/math/Rect.h"

#include <list>
#include <list>
#include <memory>

template<class T>
class Buffer;

namespace raytracer {
  class Scene;
  class Camera;
  class Primitive;
  class State;
  class Tonemap;

  /**
    * @brief The rendering engine — owns a `Camera` and a `Scene`,
    *        produces a pixel buffer.
    *
    * `Raytracer` is the top-level entry point a host application
    * (CLI tool, GUI, test) interacts with. The typical usage pattern
    * is:
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
    * `render` farms the image out to a `QThreadPool` of worker
    * threads, each handling a tile (or a strided set of rows / pixels
    * depending on the `ViewPlane` the camera is using). The
    * orchestration lives in a pimpl `Private` so the public header
    * stays free of `<QtCore>` includes — destructors are out-of-line
    * in the matching `.cpp` so the `unique_ptr<Private>` deleter sees
    * the complete type.
    *
    * The single-ray entry points (`primitiveForRay`, `rayState`,
    * `rayColor`) bypass the threading machinery and are useful for:
    *
    *  - the `Display::mousePressEvent` "click to identify primitive"
    *    path used by `SceneBrowser` and `GeneratedRayTracer`
    *  - tests that want to pin shading behaviour without spinning up
    *    a render
    *  - the `RefractingRayTracer` example's debug visualisations,
    *    which trace one ray and dump the recorded `State::events`
    *
    * `Raytracer` co-owns the `Scene` and `Camera` via
    * `std::shared_ptr` — both can outlive a render and be swapped
    * mid-flight (the `RenderWindow` rebinds the scene on file open).
    *
    * @see Camera — the projection that turns pixel coordinates into
    *      primary rays.
    * @see Scene — the geometry / lighting / ambient / background
    *      package the rays are traced against.
    * @see State — per-ray state (recursion depth, hit point, optional
    *      event log) threaded through `rayColor`.
    */
  class Raytracer : public std::enable_shared_from_this<Raytracer> {
  public:
    /**
      * Construct with a scene and a default `PinholeCamera` looking
      * at the origin from `(0, 0, -5)`. Useful for the common case
      * of "give me a basic render at sensible defaults"; for full
      * control over the camera type/position, use the two-argument
      * constructor.
      */
    explicit Raytracer(std::shared_ptr<Scene> scene);

    /**
      * Construct with a caller-supplied camera and scene. The
      * camera's view plane is left as whatever the camera's
      * constructor set it to; callers who care about pixel size /
      * sampler / interlacing should configure it themselves.
      */
    explicit Raytracer(std::shared_ptr<Camera> camera, std::shared_ptr<Scene> scene);

    virtual ~Raytracer();

    /**
      * Render the full image into a packed-RGB display buffer.
      * Internally allocates a `Buffer<Colord>` HDR accumulator,
      * runs the threading-and-tile loop into it, then applies the
      * configured `Tonemap` to produce 8-bit `unsigned int` output.
      *
      * Honours `cancel` — a render in progress will return early
      * once the in-flight tiles finish.
      *
      * Buffer dimensions must be set by the caller; the renderer does
      * not resize. Out-of-bounds tile bookkeeping is silent.
      */
    void render(Buffer<unsigned int>& buffer);

    /**
      * Render directly into an HDR `Buffer<Colord>` accumulator,
      * skipping the tonemap pass. The buffer holds raw averaged
      * radiance per pixel in `Colord` (still floats, can exceed
      * `[0, 1]`) — what an EXR writer or future path-tracing
      * accumulator wants. Use this overload when you intend to
      * post-process the float buffer yourself; use the
      * `Buffer<unsigned int>&` overload above for the convenient
      * "render and tonemap to display" path.
      */
    void render(Buffer<Colord>& buffer);

    /// @returns the tone-mapping operator applied by the
    /// `Buffer<unsigned int>&` render overload. Defaults to
    /// `LinearTonemap`, which passes HDR values through unchanged
    /// and lets `Colord::rgb()` do the clamp-and-quantize — i.e.
    /// no perceptual compression.
    std::shared_ptr<Tonemap> tonemap() const;

    /**
      * Replaces the tone-mapping operator. Pick from the registered
      * `TonemapFactory` entries: `"Linear"`, `"Reinhard"`, `"ACES"`.
      * Custom operators implementing `Tonemap::apply` are accepted
      * directly.
      */
    void setTonemap(std::shared_ptr<Tonemap> tonemap);

    /**
      * Single-ray geometry probe. Returns the `Primitive*` the ray
      * hits first (closest along `ray.direction()`), or `nullptr` if
      * the ray misses everything. Does not shade the hit, so it's
      * cheap enough for interactive picking from a mouse click.
      *
      * Pointer ownership stays with the scene; callers must not
      * delete or store across scene changes.
      */
    const Primitive* primitiveForRay(const Rayd& ray) const;

    /**
      * Single-ray probe that returns a populated `State` with the
      * `hitPoint` and recursion counters as if the ray were the
      * primary in a fresh render. Used by the example apps' debug
      * panes to surface "what did the renderer see at this pixel?"
      * without committing to a full shade.
      */
    State rayState(const Rayd& ray) const;

    /**
      * Recursive shading entry point. Returns the colour produced by
      * tracing `ray`, performing material evaluation and recursive
      * reflection / transmission as needed. Mutates `state` —
      * recursion depth, hit-point, and (when `traceEvents` is on) the
      * event log are updated in place.
      *
      * Bottoms out at `setMaximumRecursionDepth(N)` returning the
      * scene background; misses also return the scene background;
      * a hit on a primitive with no material returns black.
      */
    Colord rayColor(const Rayd& ray, State& state) const;

    /// @returns the active camera (shared ownership).
    inline std::shared_ptr<Camera> camera() const {
      return m_camera;
    }

    /// Replaces the active camera. Safe to call between renders;
    /// undefined while `render` is executing on another thread.
    inline void setCamera(std::shared_ptr<Camera> camera) {
      m_camera = camera;
    }

    /**
      * Request cancellation of an in-flight `render`. Tiles
      * already running complete before the call returns, but no
      * new tiles are scheduled. Idempotent.
      */
    void cancel();

    /// Resets the cancellation flag. Call before a fresh `render`
    /// on a renderer that was previously cancelled.
    void uncancel();

    /**
      * @returns the rectangles currently being rendered by the
      * worker threads. Used by the GUI's progress overlay to draw
      * "in-progress" stripes over the buffer; consumers must not
      * assume the list is stable across calls.
      */
    std::list<Recti> activeRects() const;

    /// @returns the active scene (shared ownership).
    inline std::shared_ptr<Scene> scene() const {
      return m_scene;
    }

    /// Replaces the scene. The new scene is rendered on the next
    /// `render` call; in-flight renders continue against the old
    /// scene until they finish.
    inline void setScene(std::shared_ptr<Scene> scene) {
      m_scene = std::move(scene);
    }

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
    std::shared_ptr<Camera> m_camera;
    std::shared_ptr<Scene> m_scene;

    struct Private;
    std::unique_ptr<Private> p;
  };
}
