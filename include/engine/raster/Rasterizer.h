#pragma once

#include "render/RenderEngine.h"

#include <atomic>
#include <list>
#include <memory>

namespace engine::raster {

/**
  * @brief `RenderEngine` that projects every mesh face's triangles
  *        to screen space, depth-tests them through a Z-buffer, and
  *        Lambertian-shades each pixel using interpolated vertex
  *        normals — the textbook software-rasterizer pipeline.
  *
  * @image html rasterizer_engine.png "Sphere through Rasterizer at default LOD"
  *
  * Pipeline:
  *
  *  1. Tessellate the scene into a single `Mesh` via
  *     `Scene::tessellate(lod)`.
  *  2. For each leaf mesh, precompute every vertex's homogeneous
  *     clip coordinate via `Camera::projectPointToClipSpace`, plus
  *     cached screen coordinates for vertices already inside the
  *     clip volume.
  *  3. Triangulate the face (fan from vertex 0 — assumes convex
  *     faces, which the per-primitive tessellate impls guarantee).
  *  4. Clip each triangle in homogeneous space against the near
  *     plane and the four viewport edges before the perspective
  *     divide.
  *  5. Rasterize each triangle via `core::rasterizeTriangle`. For
  *     every pixel inside:
  *      - Depth-test against a per-pixel Z-buffer using the
  *        perspective-correct interpolation trick (`1/z` is linear
  *        in screen space; the screen-space barycentric weights
  *        from the rasterizer give the correct interpolated depth
  *        when applied to `1/z` and inverted).
  *      - Interpolate the vertex normal and world position, again
  *        perspective-correct.
  *      - Recover a diffuse albedo from the primitive's
  *        `MatteMaterial` texture when possible, otherwise fall back
  *        to a stable per-face colour hash.
  *      - Apply Lambertian shading: `scene.ambient × ambientCoeff ×
  *        albedo + Σ_lights albedo × light.radiance × max(0, n · light.dir)`.
  *      - Write the shaded colour iff the new depth beats the
  *        existing Z-buffer cell.
  *
  * The rasterizer walks leaf primitives directly, preserving each
  * primitive's effective material before tessellation. Matte diffuse
  * textures therefore shade with their material albedo today, while
  * primitives with no usable diffuse texture still receive a stable
  * per-face fallback colour so missing materials remain visible.
  *
  * Triangles that straddle the near plane or viewport edge are
  * clipped in homogeneous space before projection so their visible
  * portion can still render without producing enormous post-divide
  * screen coordinates.
  * Face culling is switchable via `setCullMode`: the default
  * `CullMode::Both` shades both sides of every triangle, while
  * `CullMode::Back` / `CullMode::Front` skip triangles by projected
  * screen-space winding after clipping. It does not trace shadow
  * rays; lights are direct Lambertian contributions only.
  *
  * <table><tr>
  * <td>@image html rasterizer_engine_lod_0.png "lod=0"</td>
  * <td>@image html rasterizer_engine_lod_1.png "lod=1"</td>
  * <td>@image html rasterizer_engine_lod_2.png "lod=2"</td>
  * <td>@image html rasterizer_engine_lod_3.png "lod=3"</td>
  * <td>@image html rasterizer_engine_lod_4.png "lod=4"</td>
  * </tr></table>
  *
  * The interactive widget below visualises the edge-function
  * rasterization step (Pineda 1988) — the per-pixel inside-test
  * the rasterizer runs for every triangle. Drag the three vertex
  * handles to reshape the triangle and watch the filled region
  * update in real time; the dashed rectangle is the bounding box
  * the rasterizer scans; pixel colours are interpolated from the
  * three vertex colours via barycentric weights, exactly as the
  * real rasterizer would interpolate per-vertex normals or texture
  * coordinates. The production rasterizer clips triangles against
  * the homogeneous viewport before this step, then still clamps the
  * bounding box to the framebuffer as a final guard. Hover anywhere
  * to read the live `(w0, w1, w2)` weights at the cursor — outside
  * the triangle, at least one weight goes negative.
  *
  * @htmlonly
  * <script type="text/javascript" src="rasterizer_pipeline.js"></script>
  * @endhtmlonly
  *
  * Cameras supported: any subclass that overrides
  * `Camera::projectPointToClipSpace` (currently `PinholeCamera`
  * and `OrthographicCamera`). Cameras without a closed-form clip
  * projection (`FishEyeCamera`, `SphericalCamera`,
  * `EquirectangularCamera`, `ThinLensCamera`, `TiltShiftCamera`)
  * silently produce empty / degenerate renders.
  *
  * Threading: the default single-tile path streams triangles on the
  * calling thread. Setting `setQueueSize(queue)` with `queue > 1`
  * enables a tiled
  * `QThreadPool` path: projected/clipped triangles are binned by
  * tile, and each tile owns a disjoint pixel rectangle, so colour
  * and Z-buffer writes do not need locks.
  *
  * @see Wireframe — the cheaper sibling that draws only edges; the
  *      same projection + tessellation pipeline drives both.
  */
class Rasterizer : public render::RenderEngine {
public:
  enum class CullMode {
    Both,
    Back,
    Front
  };

  explicit Rasterizer(std::shared_ptr<render::Scene> scene);
  Rasterizer(std::shared_ptr<render::Camera> camera, std::shared_ptr<render::Scene> scene);

  ~Rasterizer() override;

  using RenderEngine::render;
  void render(Buffer<Colord>& buffer) override;
  void cancel() override;
  void uncancel() override;
  std::list<Recti> activeRects() const override;

  /// Level of detail forwarded to `Primitive::tessellate(lod)`.
  /// Higher values produce denser triangulation (a UV sphere at
  /// `lod=0` has 128 quads = 256 triangles; `lod=2` has 2048 quads).
  inline int lod() const { return m_lod; }
  inline void setLod(int lod) { m_lod = lod; }

  /// Sets the worker-thread count for tile rasterization. Defaults
  /// to `QThread::idealThreadCount()`.
  void setMaximumThreads(int threads);

  /// Sets the number of tiles dispatched per render. Defaults to 1;
  /// values above 1 enable the tiled `QThreadPool` path.
  void setQueueSize(int queue);

  /// Face-culling mode used after near-plane clipping and before
  /// triangle rasterization. `Both` keeps the historical two-sided
  /// behavior; `Back` and `Front` skip triangles by projected
  /// screen-space winding.
  inline CullMode cullMode() const { return m_cullMode; }
  inline void setCullMode(CullMode mode) { m_cullMode = mode; }

  /// Colour the framebuffer is cleared to before triangles are
  /// rasterized. Defaults to pure black (`Colord::black()`).
  inline const Colord& backgroundColor() const { return m_backgroundColor; }
  inline void setBackgroundColor(const Colord& color) { m_backgroundColor = color; }

private:
  struct Private;
  std::unique_ptr<Private> p;
  std::atomic<bool> m_cancelled{false};
  int m_lod{0};
  CullMode m_cullMode{CullMode::Both};
  Colord m_backgroundColor{Colord::black()};
};

}  // namespace engine::raster
