#pragma once

#include "raytracer/RenderEngine.h"

#include <atomic>

namespace raytracer {

/**
  * @brief `RenderEngine` that draws every mesh face's edges as
  *        rasterized lines on the framebuffer.
  *
  * For each primitive in the scene, calls `Primitive::tessellate(lod)`
  * to obtain a `Mesh`, projects every face vertex to screen space via
  * `Camera::projectPoint`, and rasterizes each face edge using
  * `core::drawLine` (Bresenham). No hidden-line removal — interior
  * edges are visible alongside silhouette edges. That's the V1 trade:
  * the simplest possible engine that demonstrates the
  * tessellate-and-project pipeline, useful as a debug / preview view
  * (and as a sanity check on the tessellate impls themselves —
  * mistakes in vertex ordering or LOD scheduling are immediately
  * visible).
  *
  * Cameras supported: any subclass that overrides
  * `Camera::projectPoint` (currently `PinholeCamera` and inheritors
  * `ThinLensCamera` / `TiltShiftCamera`). Cameras without a
  * closed-form inverse (e.g. `FishEyeCamera`) silently produce an
  * empty / degenerate render — `projectPoint` returns undefined for
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
class WireframeEngine : public RenderEngine {
public:
  explicit WireframeEngine(std::shared_ptr<Scene> scene);
  WireframeEngine(std::shared_ptr<Camera> camera, std::shared_ptr<Scene> scene);

  ~WireframeEngine() override;

  using RenderEngine::render;
  void render(Buffer<Colord>& buffer) override;
  void cancel() override;
  void uncancel() override;

  /// Level of detail forwarded to `Primitive::tessellate(lod)`.
  /// Higher values produce denser wireframes (a UV sphere at `lod=0`
  /// has 128 quads; `lod=2` has 2048).
  inline int lod() const { return m_lod; }
  inline void setLod(int lod) { m_lod = lod; }

  /// Colour drawn for every rasterized edge pixel. Defaults to
  /// pure white (`Colord::white()`).
  inline const Colord& edgeColor() const { return m_edgeColor; }
  inline void setEdgeColor(const Colord& color) { m_edgeColor = color; }

  /// Colour the framebuffer is cleared to before edges are drawn.
  /// Defaults to pure black (`Colord::black()`).
  inline const Colord& backgroundColor() const { return m_backgroundColor; }
  inline void setBackgroundColor(const Colord& color) { m_backgroundColor = color; }

private:
  std::atomic<bool> m_cancelled{false};
  int m_lod{0};
  Colord m_edgeColor{Colord::white()};
  Colord m_backgroundColor{Colord::black()};
};

}  // namespace raytracer
