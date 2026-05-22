#pragma once

#include "render/RenderEngine.h"
#include "core/math/Vector.h"

#include <algorithm>
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
  *  1. Walk the scene's leaf primitives via
  *     `render::Primitive::forEachLeaf`, calling `tessellate(lod)`
  *     on each so the rasterizer keeps per-primitive material
  *     associations rather than collapsing the scene into one mesh.
  *  2. For each leaf mesh, precompute every vertex's homogeneous
  *     clip coordinate via `Camera::projectPointToClipSpace`, plus
  *     cached screen coordinates for vertices already inside the
  *     clip volume.
  *  3. Triangulate the face (fan from vertex 0 — assumes convex
  *     faces, which the per-primitive tessellate impls guarantee).
  *  4. Clip each triangle in homogeneous space against the configured
  *     near/far depth interval and the four viewport edges before the
  *     perspective divide.
  *  5. Rasterize each triangle via `core::rasterizeTriangle`. Projected
  *     screen coordinates stay fractional until the edge setup converts
  *     them to fixed-point subpixel values. For every pixel inside:
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
  *        color hash.
  *      - Apply Lambertian shading: `scene.ambient × ambientCoeff ×
  *        albedo + Σ_lights albedo × light.radiance × max(0, n · light.dir)`.
  *      - Run the configured depth/stencil tests and operations.
  *        The default state is the historical Z-buffer behavior:
  *        `DepthFunc::Less`, depth writes enabled, stencil disabled.
  *      - Optionally query a rasterized directional-light shadow
  *        map before adding each diffuse light contribution.
  *      - Shade through the built-in material/Lambertian fragment
  *        path, or through a caller-provided `FragmentShader`.
  *      - Write the shaded color iff the fragment passes.
  *  6. When MSAA is enabled, repeat the coverage/depth path at a
  *     fixed 2x/4x/8x subpixel sample pattern and resolve those
  *     samples into the same float framebuffer the rest of the
  *     renderer stack tonemaps.
  *
  * The rasterizer walks leaf primitives directly, preserving each
  * primitive's effective material before tessellation. Matte diffuse
  * textures therefore shade with their material albedo today, while
  * primitives with no usable diffuse texture still receive a stable
  * per-face fallback color so missing materials remain visible.
  *
  * Triangles that straddle the configured depth interval or viewport edge are
  * clipped in homogeneous space before projection so their visible
  * portion can still render without producing enormous post-divide
  * screen coordinates.
  * Face culling is switchable via `setCullMode`: the default
  * `CullMode::Both` shades both sides of every triangle, while
  * `CullMode::Back` / `CullMode::Front` skip triangles by projected
  * screen-space winding after clipping. Depth/stencil state, opt-in
  * directional-light shadow maps, and tiny vertex/fragment shader
  * hooks are exposed for teaching the fixed-function stages without
  * forcing the default path through the programmable callbacks.
  * Shadow maps are disabled by default; with them disabled, lights
  * are direct Lambertian contributions only.
  *
  * The widget below shows the fixed-function state in the order the
  * rasterizer applies it: a first pass marks a stencil region, then
  * overlapping triangles draw through that mask while depth keeps the
  * nearest fragment. Changing the cull mode removes triangles by
  * screen-space winding before coverage reaches the depth/stencil
  * tests. This is the same pass structure later used for planar
  * reflections, mirrors, and portals.
  *
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="rasterizer_depth_stencil_cull.js"></script>
  * @endhtmlonly
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
  * green)` so interpolation errors are visible as color bends; the
  * second samples a UV-scaled checkerboard on a rotated box.
  *
  * <table><tr>
  * <td>@image html rasterizer_uv_albedo.png "UV albedo diagnostic: red = u, green = v"</td>
  * <td>@image html rasterizer_uv_checker.png "UV-mapped checkerboard on a box"</td>
  * </tr></table>
  *
  * MSAA is opt-in because it multiplies raster work. FXAA is an
  * image-space alternative that runs after the frame is complete: it
  * can smooth high-contrast edges cheaply, but unlike MSAA it cannot
  * recover hidden subpixel geometry or per-sample depth/stencil detail.
  * The comparison below renders the same high-contrast diagonal
  * triangle with raw 1x coverage, post-process FXAA, and 4x MSAA.
  *
  * <table><tr>
  * <td>@image html rasterizer_msaa_1x.png "1x raster coverage"</td>
  * <td>@image html rasterizer_post_aa_fxaa.png "1x coverage plus FXAA"</td>
  * <td>@image html rasterizer_msaa_4x.png "4x MSAA resolve"</td>
  * </tr></table>
  *
  * The widget below magnifies the same resolve operation. Drag the
  * triangle vertex handles and switch between sample counts to see why 1x
  * coverage produces only on/off pixels, while MSAA can turn a
  * partially covered pixel into a proportional gray resolve.
  * The default single-tile path keeps full-frame per-sample buffers
  * for low overhead; the queued tiled path uses tile-local color,
  * depth, and stencil sample buffers and resolves each tile directly
  * into the output framebuffer.
  *
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="rasterizer_msaa_coverage.js"></script>
  * @endhtmlonly
  *
  * Directional-light shadow maps are another opt-in quality/performance
  * feature. With them enabled, the rasterizer first renders a depth-only
  * view from each directional light, then the camera pass projects shaded
  * points into that light-space image before adding direct diffuse light.
  *
  * <table><tr>
  * <td>@image html rasterizer_shadow_maps_off.png "Direct Lambertian light only"</td>
  * <td>@image html rasterizer_shadow_maps_on.png "Directional shadow maps enabled"</td>
  * </tr></table>
  *
  * The widget below separates the two passes: the light pass stores one
  * nearest depth per texel, then the camera pass compares a draggable
  * receiver point against the stored depth plus bias. Move the caster or
  * receiver, lower the shadow-map resolution, and increase the bias to see
  * the same aliasing and shadow-detachment trade-offs exposed by the C++
  * controls.
  *
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="rasterizer_shadow_map.js"></script>
  * @endhtmlonly
  *
  * Resolution controls how finely the light-space depth image represents
  * the scene. Low values are fast but visibly quantize shadow edges; higher
  * values cost more raster work and memory.
  *
  * <table><tr>
  * <td>@image html rasterizer_shadow_map_size_32.png "32x32 shadow map"</td>
  * <td>@image html rasterizer_shadow_map_size_64.png "64x64 shadow map"</td>
  * <td>@image html rasterizer_shadow_map_size_128.png "128x128 shadow map"</td>
  * <td>@image html rasterizer_shadow_map_size_256.png "256x256 shadow map"</td>
  * <td>@image html rasterizer_shadow_map_size_512.png "512x512 shadow map"</td>
  * </tr></table>
  *
  * Cascades split the scene bounds across camera depth and build several
  * tighter directional-light maps instead of one map around the whole scene.
  * This spends more depth-pass work to improve shadow detail in near and
  * middle camera slices. Each cascade center is snapped to its light-space
  * texel grid so small camera movements do not make the shadow projection
  * drift by fractional texels.
  *
  * <table><tr>
  * <td>@image html rasterizer_shadow_cascades_1.png "1 cascade"</td>
  * <td>@image html rasterizer_shadow_cascades_2.png "2 cascades"</td>
  * <td>@image html rasterizer_shadow_cascades_4.png "4 cascades"</td>
  * </tr></table>
  *
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="rasterizer_shadow_cascades.js"></script>
  * @endhtmlonly
  *
  * Bias is a depth comparison tolerance. Too little bias can let a surface
  * shadow itself due to interpolation and quantization differences; too much
  * bias detaches shadows from their casters.
  *
  * <table><tr>
  * <td>@image html rasterizer_shadow_bias_0_030.png "bias=0.030"</td>
  * <td>@image html rasterizer_shadow_bias_0_060.png "bias=0.060"</td>
  * <td>@image html rasterizer_shadow_bias_0_080.png "bias=0.080"</td>
  * <td>@image html rasterizer_shadow_bias_0_250.png "bias=0.250"</td>
  * <td>@image html rasterizer_shadow_bias_1_500.png "bias=1.500"</td>
  * </tr></table>
  *
  * Percentage-closer filtering (PCF) softens hard texel boundaries by
  * averaging several neighboring depth comparisons around the projected
  * light-space point. Radius 0 is the exact nearest-texel comparison; radius
  * 1 uses a 3x3 kernel; radius 2 uses a 5x5 kernel.
  *
  * <table><tr>
  * <td>@image html rasterizer_shadow_filter_radius_0.png "radius=0"</td>
  * <td>@image html rasterizer_shadow_filter_radius_1.png "radius=1"</td>
  * <td>@image html rasterizer_shadow_filter_radius_2.png "radius=2"</td>
  * <td>@image html rasterizer_shadow_filter_radius_3.png "radius=3"</td>
  * <td>@image html rasterizer_shadow_filter_radius_4.png "radius=4"</td>
  * </tr></table>
  *
  * PCSS keeps the same maximum radius but makes the per-fragment
  * kernel adaptive: it first searches for blockers, estimates how far
  * the receiver is behind those blockers in light space, then grows a
  * PCF kernel up to the configured radius.
  *
  * <table><tr>
  * <td>@image html rasterizer_shadow_filter_mode_pcf.png "fixed PCF"</td>
  * <td>@image html rasterizer_shadow_filter_mode_pcss.png "blocker-search PCSS"</td>
  * </tr></table>
  *
  * The interactive widget below visualizes the edge-function
  * rasterization step (Pineda 1988) — the per-pixel inside-test
  * the rasterizer runs for every triangle. Drag the three vertex
  * handles to reshape the triangle and watch the filled region
  * update in real time; the dashed rectangle is the bounding box
  * the rasterizer scans. Toggle between barycentric vertex color
  * and UV color to see the same weights drive arbitrary attributes.
  * Edge samples use the same top-left fill rule as the C++ rasterizer,
  * so adjacent triangles assign shared-edge pixels to exactly one face.
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
  * happens. Drag any source vertex handle across the viewport boundary to see
  * generated clipped vertices and their interpolated UVs.
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
  * Threading: the default 1x single-tile path streams triangles
  * directly into the full-frame pass buffers on the calling thread.
  * MSAA and tiled rendering retain a prepared triangle set because
  * they need to reuse the same projected/clipped triangles across
  * sample offsets or tile ownership. Setting `setQueueSize(queue)`
  * with `queue > 1` enables a tiled `QThreadPool` path: projected /
  * clipped triangles are binned by tile, and each tile owns a
  * disjoint pixel rectangle, so color and Z-buffer writes do not
  * need locks. The tiled path is correctness-tested against the
  * single-tile output but is intentionally opt-in: current performance
  * is scene-dependent, and dense tessellation can still lose enough to
  * make the streaming single-tile path the predictable default. See
  * `docs/plans/rasterizer.md` Task 10 for the latest measurement
  * conclusion.
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

  enum class PostProcessAA {
    None,
    FXAA
  };

  enum class ShadowFilterMode {
    PCF,
    PCSS
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
  std::shared_ptr<render::RenderEngine> cloneForRender() const override;
  void render(Buffer<Colord>& buffer) override;
  void cancel() override;
  void uncancel() override;
  std::list<Recti> activeTiles() const override;

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

  /// Number of fixed subpixel MSAA samples per pixel. Defaults to 1
  /// (disabled). Values are rounded up to the supported 1/2/4/8
  /// sample counts.
  inline int msaaSamples() const { return m_msaaSamples; }
  void setMSAASamples(int samples);

  /// Near clip depth in the rasterizer's eye-relative depth units.
  /// Defaults to 0.1. Pinhole cameras measure this from the
  /// perspective eye; orthographic cameras measure camera-space z.
  /// Triangles straddling this depth are clipped before perspective
  /// divide, rather than dropped wholesale.
  inline double nearClipDepth() const { return m_nearClipDepth; }
  void setNearClipDepth(double depth);

  /// Far clip depth in the same eye-relative units as `nearClipDepth()`.
  /// Defaults to positive infinity, which disables far-plane clipping.
  /// Finite values clip geometry whose raster depth is greater than
  /// this value.
  inline double farClipDepth() const { return m_farClipDepth; }
  void setFarClipDepth(double depth);
  inline void clearFarClipDepth() { setFarClipDepth(std::numeric_limits<double>::infinity()); }

  /// Image-space anti-aliasing pass applied after the rasterizer has
  /// produced its float framebuffer. Defaults to `None`. FXAA is a cheap
  /// postprocess edge filter; unlike MSAA, it does not need extra coverage or
  /// depth samples, so it is useful for fast previews.
  inline PostProcessAA postProcessAA() const { return m_postProcessAA; }
  inline void setPostProcessAA(PostProcessAA aa) { m_postProcessAA = aa; }

  /// Returns whether rasterized directional-light shadow maps are enabled.
  inline bool shadowMapsEnabled() const { return m_shadowMapsEnabled; }

  /**
    * Enables rasterized directional-light shadow maps for the built-in
    * Lambertian fragment path. Custom fragment shaders are responsible for
    * their own visibility model and bypass this feature.
    *
    * <table><tr>
    * <td>@image html rasterizer_shadow_maps_off.png "disabled"</td>
    * <td>@image html rasterizer_shadow_maps_on.png "enabled"</td>
    * </tr></table>
    */
  inline void setShadowMapsEnabled(bool enabled) { m_shadowMapsEnabled = enabled; }

  /// Returns the square resolution used for each generated shadow map.
  inline int shadowMapSize() const { return m_shadowMapSize; }

  /**
    * Sets the square resolution used for each generated directional-light
    * shadow map. Defaults to 256 and is clamped to at least 1.
    *
    * <table><tr>
    * <td>@image html rasterizer_shadow_map_size_32.png "32x32"</td>
    * <td>@image html rasterizer_shadow_map_size_64.png "64x64"</td>
    * <td>@image html rasterizer_shadow_map_size_128.png "128x128"</td>
    * <td>@image html rasterizer_shadow_map_size_256.png "256x256"</td>
    * <td>@image html rasterizer_shadow_map_size_512.png "512x512"</td>
    * </tr></table>
    */
  void setShadowMapSize(int size);

  /// Returns how many camera-depth slices each directional light gets.
  /// The default is 1, which is the original single shadow-map behavior.
  inline int shadowCascadeCount() const { return m_shadowCascadeCount; }

  /**
    * Sets the number of cascaded shadow maps per directional light.
    * Values are clamped to the range [1, 4]. Counts above 1 split the
    * scene bounds by camera view depth and build one tighter light-space
    * shadow map per slice. Cascade centers are snapped to the shadow-map
    * texel grid for stable camera previews.
    *
    * <table><tr>
    * <td>@image html rasterizer_shadow_cascades_1.png "1 cascade"</td>
    * <td>@image html rasterizer_shadow_cascades_2.png "2 cascades"</td>
    * <td>@image html rasterizer_shadow_cascades_4.png "4 cascades"</td>
    * </tr></table>
    */
  inline void setShadowCascadeCount(int count) {
    m_shadowCascadeCount = std::clamp(count, 1, 4);
  }

  /// Returns the depth bias used by the shadow-map comparison.
  inline double shadowBias() const { return m_shadowBias; }

  /**
    * Sets the depth bias added during the shadow-map comparison to avoid
    * self-shadowing from small interpolation differences.
    *
    * <table><tr>
    * <td>@image html rasterizer_shadow_bias_0_030.png "0.030"</td>
    * <td>@image html rasterizer_shadow_bias_0_060.png "0.060"</td>
    * <td>@image html rasterizer_shadow_bias_0_080.png "0.080"</td>
    * <td>@image html rasterizer_shadow_bias_0_250.png "0.250"</td>
    * <td>@image html rasterizer_shadow_bias_1_500.png "1.500"</td>
    * </tr></table>
    */
  inline void setShadowBias(double bias) { m_shadowBias = std::max(0.0, bias); }

  /// Returns the maximum filter radius in shadow-map texels. Radius 0 is a
  /// hard nearest-texel shadow comparison; radius 1 is a 3x3 kernel, etc.
  inline int shadowFilterRadius() const { return m_shadowFilterRadius; }

  /**
    * Sets the percentage-closer filtering radius in shadow-map texels.
    * The value is clamped to at least 0. Filtering averages neighboring
    * depth-test results and therefore softens shadow-map texel edges; it is
    * not physically based area-light softness.
    *
    * <table><tr>
    * <td>@image html rasterizer_shadow_filter_radius_0.png "0"</td>
    * <td>@image html rasterizer_shadow_filter_radius_1.png "1"</td>
    * <td>@image html rasterizer_shadow_filter_radius_2.png "2"</td>
    * <td>@image html rasterizer_shadow_filter_radius_3.png "3"</td>
    * <td>@image html rasterizer_shadow_filter_radius_4.png "4"</td>
    * </tr></table>
    */
  inline void setShadowFilterRadius(int radius) { m_shadowFilterRadius = std::max(0, radius); }

  /// Returns the active shadow-map filter. PCF uses the configured
  /// radius directly; PCSS searches blockers first and derives a
  /// receiver-local penumbra radius, clamped by `shadowFilterRadius()`.
  inline ShadowFilterMode shadowFilterMode() const { return m_shadowFilterMode; }

  /**
    * Sets the shadow-map filter mode. PCF keeps one fixed percentage-closer
    * kernel. PCSS performs a blocker search over the configured radius, then
    * uses a smaller or larger PCF kernel based on receiver-vs-blocker depth.
    *
    * <table><tr>
    * <td>@image html rasterizer_shadow_filter_mode_pcf.png "PCF"</td>
    * <td>@image html rasterizer_shadow_filter_mode_pcss.png "PCSS"</td>
    * </tr></table>
    */
  inline void setShadowFilterMode(ShadowFilterMode mode) { m_shadowFilterMode = mode; }

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

  // `backgroundColor()`, `setBackgroundColor`, `clearBackgroundColor`,
  // and `hasBackgroundColorOverride` are inherited from `render::RenderEngine`.
  // The Rasterizer uses the default fallback: when no override is set,
  // the framebuffer is cleared to the scene's `background()`.

private:
  struct Private;
  std::unique_ptr<Private> p;
  std::atomic<bool> m_cancelled{false};
  int m_lod{0};
  int m_msaaSamples{1};
  double m_nearClipDepth{0.1};
  double m_farClipDepth{std::numeric_limits<double>::infinity()};
  PostProcessAA m_postProcessAA{PostProcessAA::None};
  bool m_shadowMapsEnabled{false};
  int m_shadowMapSize{256};
  int m_shadowCascadeCount{1};
  double m_shadowBias{1e-3};
  int m_shadowFilterRadius{0};
  ShadowFilterMode m_shadowFilterMode{ShadowFilterMode::PCF};
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
};

}  // namespace engine::raster
