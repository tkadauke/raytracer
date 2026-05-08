#pragma once

#include "render/RenderEngine.h"
#include "core/math/Vector.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <utility>

namespace render {
  class Material;
  class Primitive;
}

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
  *      - Interpolate the vertex normal, world position, and UV
  *        coordinates, again perspective-correct.
  *      - Recover a diffuse albedo from the primitive's
  *        `MatteMaterial` texture with the interpolated hit context
  *        when possible, otherwise fall back to a stable per-face
  *        colour hash.
  *      - Apply Lambertian shading: `scene.ambient × ambientCoeff ×
  *        albedo + Σ_lights albedo × light.radiance × max(0, n · light.dir)`.
  *      - Run the configured depth/stencil tests and operations.
  *        The default state is the historical Z-buffer behaviour:
  *        `DepthFunc::Less`, depth writes enabled, stencil disabled.
  *      - Shade through the built-in material/Lambertian fragment
  *        path, or through a caller-provided `FragmentShader`.
  *      - Write the shaded colour iff the fragment passes.
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
  * screen-space winding after clipping. Depth/stencil state and
  * tiny vertex/fragment shader hooks are exposed for teaching the
  * fixed-function stages without forcing the default path through
  * the programmable callbacks. It does not trace shadow rays;
  * lights are direct Lambertian contributions only.
  *
  * <table><tr>
  * <td>@image html rasterizer_engine_lod_0.png "lod=0"</td>
  * <td>@image html rasterizer_engine_lod_1.png "lod=1"</td>
  * <td>@image html rasterizer_engine_lod_2.png "lod=2"</td>
  * <td>@image html rasterizer_engine_lod_3.png "lod=3"</td>
  * <td>@image html rasterizer_engine_lod_4.png "lod=4"</td>
  * </tr></table>
  *
  * Interpolated UVs feed the same material texture path as ray-hit
  * positions. The first image maps `(u, v)` directly to `(red,
  * green)` so interpolation errors are visible as colour bends; the
  * second samples a UV-scaled checkerboard on a rotated box.
  *
  * <table><tr>
  * <td>@image html rasterizer_uv_albedo.png "UV albedo diagnostic: red = u, green = v"</td>
  * <td>@image html rasterizer_uv_checker.png "UV-mapped checkerboard on a box"</td>
  * </tr></table>
  *
  * The interactive widget below visualises the edge-function
  * rasterization step (Pineda 1988) — the per-pixel inside-test
  * the rasterizer runs for every triangle. Drag the three vertex
  * handles to reshape the triangle and watch the filled region
  * update in real time; the dashed rectangle is the bounding box
  * the rasterizer scans. Toggle between barycentric vertex colour
  * and UV colour to see the same weights drive arbitrary attributes.
  * The production rasterizer clips triangles against the homogeneous
  * viewport before this step, then still clamps the bounding box to
  * the framebuffer as a final guard. Hover anywhere to read the live
  * `(w0, w1, w2)` weights and interpolated UVs at the cursor —
  * outside the triangle, at least one weight goes negative.
  *
  * @htmlonly
  * <script type="text/javascript" src="rasterizer_pipeline.js"></script>
  * @endhtmlonly
  *
  * Perspective projection makes screen-space barycentric weights
  * affine in screen space, not in camera space. The widget below
  * compares a tilted textured quad using affine UV interpolation
  * versus the rasterizer's perspective-correct `1/z` interpolation.
  *
  * @htmlonly
  * <script type="text/javascript" src="rasterizer_perspective_uv.js"></script>
  * @endhtmlonly
  *
  * Clipping creates new vertices on viewport or near-plane edges.
  * Those generated vertices must carry interpolated attributes too;
  * otherwise UVs and normals would jump exactly where clipping
  * happens. The widget below moves one triangle vertex outside the
  * viewport and shows the generated clipped vertices with their UVs.
  *
  * @htmlonly
  * <script type="text/javascript" src="rasterizer_clip_attributes.js"></script>
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

  enum class DepthFunc {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    GreaterEqual,
    NotEqual,
    Always
  };

  enum class StencilFunc {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    GreaterEqual,
    NotEqual,
    Always
  };

  enum class StencilOp {
    Keep,
    Zero,
    Replace,
    IncrementClamp,
    DecrementClamp,
    Invert
  };

  struct VertexInput {
    Vector3d worldPosition;
    Vector3d normal;
    Vector2d uv;
    Vector4d clipPosition;
    Vector3d screenPosition;
    const render::Primitive* primitive;
    const render::Material* material;
    std::uint64_t faceIdx;
  };

  struct VertexOutput {
    Vector3d worldPosition;
    Vector3d normal;
    Vector2d uv;
    Vector4d clipPosition;
    Vector3d screenPosition;
  };

  struct FragmentInput {
    int x;
    int y;
    double depth;
    Vector3d barycentric;
    Vector3d worldPosition;
    Vector3d normal;
    Vector2d uv;
    const render::Primitive* primitive;
    const render::Material* material;
    std::uint64_t faceIdx;
  };

  using VertexShader = std::function<VertexOutput(const VertexInput&)>;
  using FragmentShader = std::function<Colord(const FragmentInput&)>;

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

  /// Depth comparison applied after coverage and stencil tests.
  /// Defaults to `Less`, matching the original Z-buffer path.
  inline DepthFunc depthFunc() const { return m_depthFunc; }
  inline void setDepthFunc(DepthFunc func) { m_depthFunc = func; }

  /// Initial value for the per-render depth buffer. Defaults to
  /// positive infinity so `DepthFunc::Less` accepts the first
  /// visible fragment at each pixel.
  inline double depthClearValue() const { return m_depthClearValue; }
  inline void setDepthClearValue(double value) { m_depthClearValue = value; }

  /// Controls whether passing fragments update the depth buffer.
  /// Defaults to true. Disabling writes keeps depth testing active
  /// but makes later geometry compare against the old depth value.
  inline bool depthWriteEnabled() const { return m_depthWriteEnabled; }
  inline void setDepthWriteEnabled(bool enabled) { m_depthWriteEnabled = enabled; }

  /// Optional 8-bit stencil test. Disabled by default; when enabled,
  /// the reference and stored stencil values are compared after
  /// applying `stencilMask`.
  inline bool stencilTestEnabled() const { return m_stencilTestEnabled; }
  inline void setStencilTestEnabled(bool enabled) { m_stencilTestEnabled = enabled; }

  inline StencilFunc stencilFunc() const { return m_stencilFunc; }
  inline std::uint8_t stencilReference() const { return m_stencilReference; }
  inline std::uint8_t stencilMask() const { return m_stencilMask; }
  inline void setStencilFunc(StencilFunc func, std::uint8_t reference, std::uint8_t mask = 0xFF) {
    m_stencilFunc = func;
    m_stencilReference = reference;
    m_stencilMask = mask;
  }

  inline std::uint8_t stencilClearValue() const { return m_stencilClearValue; }
  inline void setStencilClearValue(std::uint8_t value) { m_stencilClearValue = value; }

  inline std::uint8_t stencilWriteMask() const { return m_stencilWriteMask; }
  inline void setStencilWriteMask(std::uint8_t mask) { m_stencilWriteMask = mask; }

  inline StencilOp stencilFailOp() const { return m_stencilFailOp; }
  inline StencilOp stencilDepthFailOp() const { return m_stencilDepthFailOp; }
  inline StencilOp stencilPassOp() const { return m_stencilPassOp; }
  inline void setStencilOps(StencilOp stencilFail, StencilOp depthFail, StencilOp pass) {
    m_stencilFailOp = stencilFail;
    m_stencilDepthFailOp = depthFail;
    m_stencilPassOp = pass;
  }

  /// Optional programmable vertex stage over already-projected mesh
  /// attributes. Return an adjusted `VertexOutput`, or leave unset
  /// for the built-in pass-through stage.
  inline const VertexShader& vertexShader() const { return m_vertexShader; }
  inline void setVertexShader(VertexShader shader) { m_vertexShader = std::move(shader); }
  inline void clearVertexShader() { m_vertexShader = VertexShader(); }

  /// Optional fragment stage over the perspective-correct interpolated
  /// attributes. Leave unset for the built-in material/Lambertian
  /// fragment shading path.
  inline const FragmentShader& fragmentShader() const { return m_fragmentShader; }
  inline void setFragmentShader(FragmentShader shader) { m_fragmentShader = std::move(shader); }
  inline void clearFragmentShader() { m_fragmentShader = FragmentShader(); }

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
  DepthFunc m_depthFunc{DepthFunc::Less};
  double m_depthClearValue{std::numeric_limits<double>::infinity()};
  bool m_depthWriteEnabled{true};
  bool m_stencilTestEnabled{false};
  StencilFunc m_stencilFunc{StencilFunc::Always};
  std::uint8_t m_stencilReference{0};
  std::uint8_t m_stencilMask{0xFF};
  std::uint8_t m_stencilClearValue{0};
  std::uint8_t m_stencilWriteMask{0xFF};
  StencilOp m_stencilFailOp{StencilOp::Keep};
  StencilOp m_stencilDepthFailOp{StencilOp::Keep};
  StencilOp m_stencilPassOp{StencilOp::Keep};
  VertexShader m_vertexShader;
  FragmentShader m_fragmentShader;
  Colord m_backgroundColor{Colord::black()};
};

}  // namespace engine::raster
