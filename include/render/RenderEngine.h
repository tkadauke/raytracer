#pragma once

#include "core/Color.h"
#include "core/math/Rect.h"

#include <list>
#include <memory>
#include <optional>

template<class T>
class Buffer;

namespace render {
  class Tonemap;
  class Camera;
  class Integrator;
  class Scene;

  /**
    * @brief Abstract base for all rendering backends — what every
    *        engine has in common, regardless of how it actually
    *        produces pixels.
    *
    * Subclasses are concrete engines:
    *
    *  - `Raytracer` — Whitted-style recursive raytracer. Shoots
    *    rays through pixels, traces reflection / refraction recursively.
    *  - `Wireframe` — edge projection from `Mesh::tessellate`
    *    outputs, near-plane line clipping, then Bresenham line drawing.
    *  - `Rasterizer` — software triangle rasterizer with clipping,
    *    depth/stencil state, material shading, MSAA, and shadow maps.
    *  - Future: `OpenGLEngine` (real-time GL viewport),
    *    `PathTracerEngine` (Monte Carlo over the same scene graph).
    *
    * The base owns:
    *
    *  - **Camera + scene** — every engine needs them. Held as
    *    `shared_ptr` so callers can swap mid-flight (the GUI's
    *    `RenderWindow` rebinds the scene on file open).
    *  - **Tonemap** — every engine producing LDR display output
    *    runs through one. The two-overload render API
    *    (`Buffer<Colord>` HDR + `Buffer<unsigned int>` LDR) lives
    *    here: subclasses implement the HDR variant; the LDR
    *    overload is a shared tonemap wrapper.
    *  - **Cancellation + display progress** — `cancel` / `uncancel`,
    *    `activeTiles` for the in-progress UI overlay, and
    *    `completedTiles` for publishing finished tiles into the GUI's
    *    immutable front buffer. The actual cancellation mechanism is
    *    engine-specific (raytracer flips a per-camera flag); the base
    *    just exposes the abstract hooks.
    *
    * What does NOT live here:
    *
    *  - **Single-ray probes** (`rayColor`, `rayState`,
    *    `primitiveForRay`). These are raytracer-specific — a
    *    wireframe engine has no notion of "what colour is this
    *    pixel via recursive ray tracing."
    *  - **Single-ray radiance policy.** Lives behind
    *    `Integrator`, which receives a borrowed `Scene`, `Rayd`,
    *    and mutable `State` for one ray. A `RenderEngine` owns
    *    image production; an integrator owns "what does this ray
    *    see?"
    *  - **Recursion-depth limit.** Lives on `Raytracer` today
    *    because only ray-recursive engines (raytracer, future path
    *    tracer) have a meaningful concept of recursion. It belongs
    *    with the concrete ray integration policy as that layer is
    *    factored out.
    *  - **Worker threads.** Each engine picks its own threading
    *    strategy — the raytracer tiles pixels, the software rasterizer
    *    can tile the framebuffer, wireframe runs on the caller thread,
    *    and the GL engine submits to the driver.
    *
    * @see engine::raytracer::Raytracer — recursive raytracing engine.
    * @see engine::wireframe::Wireframe — edge-only tessellation preview.
    * @see engine::raster::Rasterizer — software triangle rasterizer.
    * @see Integrator — single-ray radiance evaluation contract.
    * @see Tonemap — the HDR-to-LDR operator the LDR-render
    *      overload runs through.
    */
  class RenderEngine : public std::enable_shared_from_this<RenderEngine> {
  public:
    explicit RenderEngine(std::shared_ptr<render::Scene> scene);
    explicit RenderEngine(std::shared_ptr<render::Camera> camera,
                          std::shared_ptr<render::Scene> scene);

    virtual ~RenderEngine();

    /**
      * Creates an isolated engine instance for a render thread.
      *
      * GUI display widgets use this to give each render thread its
      * own immutable engine snapshot. That lets a stale render be
      * cancelled and left to drain while the next interactive frame
      * starts immediately from a newer camera / scene state.
      *
      * Returning `nullptr` opts out: callers must serialize renders
      * against this engine because the implementation cannot be safely
      * run concurrently with a later render.
      */
    virtual std::shared_ptr<RenderEngine> cloneForRender() const;

    /**
      * Render the full image into a packed-RGB display buffer.
      *
      * Default implementation: allocates a `Buffer<Colord>` HDR
      * accumulator, dispatches to the engine-specific
      * `render(Buffer<Colord>&)` virtual, then applies the configured
      * `Tonemap` once at the end. Simple but blocks the display
      * buffer empty until the very end of the render.
      *
      * Engines whose interactive consumers want progressive display
      * (the GUI's `RenderWidget` polls the buffer between frames)
      * override this with a path that writes tonemapped values into
      * the LDR buffer as the workers complete pixels — see
      * `Raytracer::render(Buffer<unsigned int>&)` for the canonical
      * implementation.
      *
      * Buffer dimensions must be set by the caller; engines do not
      * resize.
      */
    virtual void render(Buffer<unsigned int>& buffer);

    /**
      * Engine-specific HDR render path. Concrete engines override
      * this with their actual rendering loop — the raytracer tiles
      * pixels and shoots rays; a wireframe engine projects edges; a
      * software rasterizer runs the scanline pipeline.
      *
      * The buffer holds raw averaged radiance per pixel as `Colord`
      * (still floats, can exceed `[0, 1]`) — the form an EXR writer
      * or future path-tracing accumulator wants. Direct callers
      * skip the LDR tonemap that the `Buffer<unsigned int>&`
      * overload applies.
      */
    virtual void render(Buffer<Colord>& buffer) = 0;

    /// @returns the active camera (shared ownership).
    inline std::shared_ptr<render::Camera> camera() const {
      return m_camera;
    }

    /// Replaces the active camera. Safe to call between renders. If
    /// a GUI render is running on a `cloneForRender()` snapshot, that
    /// snapshot keeps its old camera; otherwise callers must not
    /// mutate the engine while `render` is executing on another
    /// thread.
    void setCamera(std::shared_ptr<render::Camera> camera);

    /// @returns the active scene (shared ownership).
    inline std::shared_ptr<render::Scene> scene() const {
      return m_scene;
    }

    /// Replaces the scene. The new scene is rendered on the next
    /// `render` call. If a GUI render is running on a
    /// `cloneForRender()` snapshot, that snapshot keeps its old scene;
    /// otherwise callers must not mutate the engine while `render` is
    /// executing on another thread.
    inline void setScene(std::shared_ptr<render::Scene> scene) {
      m_scene = std::move(scene);
    }

    /// @returns the tone-mapping operator the LDR render overload
    /// applies. Defaults to `LinearTonemap` (pass-through).
    std::shared_ptr<render::Tonemap> tonemap() const;

    /// Replaces the tone-mapping operator. Pick from the registered
    /// `TonemapFactory` entries (`"Linear"`, `"Reinhard"`, `"ACES"`)
    /// or supply a custom subclass.
    void setTonemap(std::shared_ptr<render::Tonemap> tonemap);

    /**
      * Enables or disables in-flight writes meant only for interactive display.
      *
      * GUI preview widgets poll the render buffer while a render is running, so
      * progressive-capable engines normally publish partial results before the
      * final image is complete. Batch callers such as rendercli can disable
      * those intermediate writes and keep the same final output with less
      * scheduler overhead.
      */
    void setProgressiveDisplayEnabled(bool enabled);

    /// @returns true when engines should publish in-flight display updates.
    bool progressiveDisplayEnabled() const;

    void setAnimationFrame(double frame);
    double animationFrame() const;
    void setShutterInterval(double open, double close);
    double shutterOpen() const;
    double shutterClose() const;

    /**
      * Request cancellation of an in-flight render. Engines stop
      * scheduling new work after this; in-progress tiles or
      * subdivisions complete before the call returns. Idempotent.
      */
    virtual void cancel() = 0;

    /// Resets the cancellation flag. Call before a fresh render on
    /// an engine that was previously cancelled.
    virtual void uncancel() = 0;

    /// @returns framebuffer tile bounds the engine is currently
    /// working on. Used by the GUI's progress overlay to draw
    /// stripes over still-rendering tiles. Default: empty list
    /// (engines without tile bookkeeping return nothing).
    virtual std::list<Recti> activeTiles() const;

    /// @returns framebuffer tile bounds the engine has finished
    /// writing. `RenderWidget` uses these as dirty-tile publication
    /// hints: completed tiles are copied from the render thread's
    /// back buffer into the UI thread's front image, while in-progress
    /// tiles are left untouched until done. Engines without
    /// progressive LDR tile output return an empty list and publish
    /// the full image only when `render()` finishes.
    virtual std::list<Recti> completedTiles() const;

    /// @returns the colour the framebuffer is cleared to before the
    /// engine writes any fragments. Default implementation returns
    /// the explicit override if one was set via `setBackgroundColor`,
    /// otherwise falls back to the attached scene's `background()`,
    /// or black if there is no scene either. Subclasses may override
    /// to change the no-override fallback — `Wireframe` returns black
    /// rather than the scene background so its line-on-black look is
    /// preserved by default.
    virtual Colord backgroundColor() const;

    /// Sets an explicit override for `backgroundColor()`. Survives
    /// `clearBackgroundColor` and `cloneForRender` propagation paths.
    void setBackgroundColor(const Colord& color);

    /// Drops the explicit override so `backgroundColor()` falls back
    /// to its subclass-chosen default again.
    void clearBackgroundColor();

    /// @returns true iff `setBackgroundColor` has been called more
    /// recently than `clearBackgroundColor`. Engines use this to decide
    /// whether to propagate the override through `cloneForRender`.
    bool hasBackgroundColorOverride() const;

  protected:
    /// Copies base render settings into a cloned concrete engine.
    void copyRenderEngineStateTo(RenderEngine& engine) const;

    std::shared_ptr<render::Camera> m_camera;
    std::shared_ptr<render::Scene> m_scene;
    std::optional<Colord> m_backgroundColorOverride;

  private:
    struct Private;
    std::unique_ptr<Private> p;
  };
}
