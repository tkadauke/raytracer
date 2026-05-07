#pragma once

#include "render/RenderEngine.h"

#include <atomic>

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
  *  2. For every face, project its vertices to screen space + depth
  *     via `Camera::projectPointWithDepth` — the depth value is the
  *     eye-relative distance along the camera's forward axis.
  *  3. Triangulate the face (fan from vertex 0 — assumes convex
  *     faces, which the per-primitive tessellate impls guarantee).
  *  4. Rasterize each triangle via `core::rasterizeTriangle`. For
  *     every pixel inside:
  *      - Depth-test against a per-pixel Z-buffer using the
  *        perspective-correct interpolation trick (`1/z` is linear
  *        in screen space; the screen-space barycentric weights
  *        from the rasterizer give the correct interpolated depth
  *        when applied to `1/z` and inverted).
  *      - Interpolate the vertex normal and world position, again
  *        perspective-correct.
  *      - Apply Lambertian shading: `scene.ambient × ambientCoeff +
  *        Σ_lights faceColor × light.radiance × max(0, n · light.dir)`.
  *      - Write the shaded colour iff the new depth beats the
  *        existing Z-buffer cell.
  *
  * Each face is coloured by an index hash rather than the
  * primitive's material — recovering per-primitive material from the
  * merged mesh requires either UV-interpolated texture sampling or a
  * primitive-tracking path through the tessellation. The hash
  * approach gives recognisably-shaded objects (the unlit side is
  * dim, the lit side bright) without the material-tracking
  * complexity. Replacing the hash with material albedo is a future
  * improvement.
  *
  * Triangles with any vertex behind the eye are dropped entirely —
  * the rasterizer doesn't yet implement near-plane clipping, so a
  * triangle that straddles the near plane is invisible. Backface
  * culling is also pending; the rasterizer currently shades both
  * sides of every triangle.
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
  * coordinates. Hover anywhere to read the live `(w0, w1, w2)`
  * weights at the cursor — outside the triangle, at least one
  * weight goes negative.
  *
  * @htmlonly
  * <script type="text/javascript" src="rasterizer_pipeline.js"></script>
  * @endhtmlonly
  *
  * Cameras supported: any subclass that overrides
  * `Camera::projectPoint` (currently `PinholeCamera` and
  * inheritors `ThinLensCamera` / `TiltShiftCamera`). Cameras
  * without a closed-form inverse (`FishEyeCamera`,
  * `SphericalCamera`, `EquirectangularCamera`) silently produce
  * empty / degenerate renders.
  *
  * Threading: V1 renders on the calling thread.
  *
  * @see Wireframe — the cheaper sibling that draws only edges; the
  *      same projection + tessellation pipeline drives both.
  */
class Rasterizer : public render::RenderEngine {
public:
  explicit Rasterizer(std::shared_ptr<render::Scene> scene);
  Rasterizer(std::shared_ptr<render::Camera> camera, std::shared_ptr<render::Scene> scene);

  ~Rasterizer() override;

  using RenderEngine::render;
  void render(Buffer<Colord>& buffer) override;
  void cancel() override;
  void uncancel() override;

  /// Level of detail forwarded to `Primitive::tessellate(lod)`.
  /// Higher values produce denser triangulation (a UV sphere at
  /// `lod=0` has 128 quads = 256 triangles; `lod=2` has 2048 quads).
  inline int lod() const { return m_lod; }
  inline void setLod(int lod) { m_lod = lod; }

  /// Colour the framebuffer is cleared to before triangles are
  /// rasterized. Defaults to pure black (`Colord::black()`).
  inline const Colord& backgroundColor() const { return m_backgroundColor; }
  inline void setBackgroundColor(const Colord& color) { m_backgroundColor = color; }

private:
  std::atomic<bool> m_cancelled{false};
  int m_lod{0};
  Colord m_backgroundColor{Colord::black()};
};

}  // namespace engine::raster
