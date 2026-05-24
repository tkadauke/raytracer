#pragma once

#include "render/RenderEngine.h"

#include <atomic>

namespace engine::wireframe {

  /**
  * @brief `RenderEngine` that draws every mesh face's edges as
  *        rasterized lines on the framebuffer.
  *
  * @image html wireframe_engine.png "Sphere through Wireframe at default LOD"
  *
  * For each primitive in the scene, calls `Primitive::tessellate(lod)`
  * to obtain a `Mesh`, clips each face edge against the camera's near
  * clip depth, projects the surviving endpoints via `Camera::projectPoint`,
  * and rasterizes each edge using `core::drawLine` (Bresenham). No
  * hidden-line removal — interior edges are visible alongside silhouette
  * edges. That's the trade:
  * the simplest possible engine that demonstrates the
  * tessellate-and-project pipeline, useful as a debug / preview view
  * (and as a sanity check on the tessellate impls themselves —
  * mistakes in vertex ordering or LOD scheduling are immediately
  * visible).
  *
  * The `lod` knob is forwarded to every primitive's `tessellate`. As
  * lod climbs, the segment count doubles per dimension, so vertex
  * counts grow roughly 4× per step on 2D-parameterised primitives
  * (sphere, torus). Past a certain density every visible pixel falls
  * on an edge and the wireframe saturates to a solid silhouette —
  * that's the point at which switching to a shaded engine becomes
  * the natural next step.
  *
  * <table><tr>
  * <td>@image html wireframe_engine_lod_0.png "lod=0"</td>
  * <td>@image html wireframe_engine_lod_1.png "lod=1"</td>
  * <td>@image html wireframe_engine_lod_2.png "lod=2"</td>
  * <td>@image html wireframe_engine_lod_3.png "lod=3"</td>
  * <td>@image html wireframe_engine_lod_4.png "lod=4 (saturated)"</td>
  * </tr></table>
  *
  * Cameras supported: any subclass that overrides
  * `Camera::projectPoint` (currently `PinholeCamera`,
  * `OrthographicCamera`, and the pinhole-compatible forward projection
  * fallback exposed by `ThinLensCamera` / `TiltShiftCamera`). Cameras
  * without a closed-form inverse (e.g. `FishEyeCamera`) silently produce
  * an empty / degenerate render — `projectPoint` returns undefined for
  * every point, so every edge is dropped.
  *
  * Threading: V1 renders on the calling thread. Edge counts for
  * tessellated scenes are bounded (a UV sphere at `lod = 0` produces
  * 128 quads × 4 edges = 512 edges; even a torus at `lod = 2` is
  * only ~16k edges) and Bresenham is fast in absolute terms, so
  * single-threaded is fine for typical scene sizes. Tile / edge
  * parallelisation is a V2 win once a wireframe-heavy workload
  * shows up.
  *
  * Cancellation: `cancel()` flips an atomic flag the inner loop
  * checks before each face. Exits gracefully — partial edges remain
  * on screen, but no in-progress edge is left half-drawn.
  */
  class Wireframe : public render::RenderEngine {
  public:
    explicit Wireframe(std::shared_ptr<render::Scene> scene);
    Wireframe(std::shared_ptr<render::Camera> camera, std::shared_ptr<render::Scene> scene);

    ~Wireframe() override;

    using RenderEngine::render;
    std::shared_ptr<render::RenderEngine> cloneForRender() const override;
    void render(Buffer<Colord>& buffer) override;
    void cancel() override;
    void uncancel() override;

    /// Level of detail forwarded to `Primitive::tessellate(lod)`.
    /// Higher values produce denser wireframes (a UV sphere at `lod=0`
    /// has 128 quads; `lod=2` has 2048).
    inline int lod() const {
      return m_lod;
    }
    inline void setLod(int lod) {
      m_lod = lod;
    }

    /// Colour drawn for every rasterized edge pixel. Defaults to
    /// pure white (`Colord::white()`).
    inline const Colord& edgeColor() const {
      return m_edgeColor;
    }
    inline void setEdgeColor(const Colord& color) {
      m_edgeColor = color;
    }

    /// Colour the framebuffer is cleared to before edges are drawn.
    /// Defaults to pure black — the Wireframe overrides the
    /// `RenderEngine` default (which would fall back to the scene's
    /// `background()`) so the canonical lines-on-black look survives
    /// even when the scene carries a colourful background. Call
    /// `setBackgroundColor` to install an explicit override or copy
    /// the scene's background back in.
    Colord backgroundColor() const override;

    /// Eye-relative near clip depth. Edges with both endpoints closer
    /// than this value are skipped; edges that cross the depth are
    /// shortened to the near plane before projection. Defaults to 0.1.
    inline double nearClipDepth() const {
      return m_nearClipDepth;
    }
    void setNearClipDepth(double depth);

  private:
    std::atomic<bool> m_cancelled{false};
    int m_lod{0};
    Colord m_edgeColor{Colord::white()};
    double m_nearClipDepth{0.1};
  };

} // namespace engine::wireframe
