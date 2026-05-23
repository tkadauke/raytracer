#pragma once

#include "render/RenderEngine.h"
#include "core/math/Vector.h"

#include <algorithm>
#include <atomic>
#include <cmath>
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
  *        direct-shades each pixel from interpolated vertex normals
  *        and material data — the textbook software-rasterizer pipeline.
  *
  * The default render below is intentionally plain: one tessellated sphere,
  * projected, depth-tested, shaded, and written to the framebuffer. The rest
  * of this page then changes one rasterizer stage at a time so each image or
  * widget has a single job.
  *
  * @image html rasterizer_engine.png "Sphere through Rasterizer at default LOD"
  *
  * Pipeline:
  *
  *  1. Walk the scene's leaf primitives via
  *     `render::Primitive::forEachLeaf`. Finite primitive bounds
  *     wholly outside one clip plane are rejected before tessellation;
  *     otherwise the rasterizer calls `tessellate(lod)` on each leaf
  *     so it keeps per-primitive material associations rather than
  *     collapsing the scene into one mesh.
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
  *      - Recover a diffuse albedo and direct-lighting coefficients from
  *        `MatteMaterial` / `PhongMaterial` with the interpolated hit context
  *        when possible, otherwise fall back to a stable per-face color hash.
  *      - Apply direct material shading: ambient plus diffuse, with a
  *        Phong specular highlight when the material provides one.
  *      - Run the configured depth/stencil tests and operations.
  *        The default state is the historical Z-buffer behavior:
  *        `DepthFunc::Less`, depth writes enabled, stencil disabled.
  *      - Optionally query a rasterized directional-light shadow
  *        map before adding each diffuse light contribution.
  *      - Shade through the built-in material fragment path, or through a
  *        caller-provided `FragmentShader`.
  *      - Write the shaded color iff the fragment passes.
  *  6. When MSAA is enabled, repeat the coverage/depth path at a
  *     fixed 2x/4x/8x subpixel sample pattern and resolve those
  *     samples into the same float framebuffer the rest of the
  *     renderer stack tonemaps.
  *
  * The rasterizer walks leaf primitives directly, preserving each
  * primitive's effective material before tessellation. Matte diffuse
  * textures shade with their material albedo and coefficients, Phong
  * materials add local specular highlights, and primitives with no usable
  * diffuse texture still receive a stable per-face fallback color so
  * missing materials remain visible.
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
  * forcing the default path through the programmable callbacks. The final
  * color-output stage supports RGB write masks and fixed-function blending;
  * alpha-style composition is currently pass-constant because `Colord` has no
  * stored alpha channel.
  * Shadow maps are disabled by default; with them disabled, lights are direct
  * material contributions only.
  *
  * @par Fixed-function tests and color output
  *
  * The first two widgets are about pass state rather than mesh shape. Read them
  * from left to right in pipeline order: coverage creates candidate fragments,
  * culling can reject whole triangles, stencil and depth decide which fragments
  * survive, depth bias can nudge those fragments forward or backward for
  * multi-pass layering, and color output decides how a surviving RGB value
  * changes the framebuffer.
  *
  * The widget below shows the fixed-function state in the order the
  * rasterizer applies it: a first pass marks a stencil region, then
  * overlapping triangles draw through that mask while depth keeps the
  * nearest fragment. Changing the cull mode removes triangles by
  * screen-space winding before coverage reaches the depth/stencil
  * tests. The same stencil-then-draw shape is the basis for planar
  * reflections, mirrors, portals, decals, and selection outlines.
  *
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="rasterizer_depth_stencil_cull.js"></script>
  * @endhtmlonly
  *
  * The color-output widget shows the final pass step: optional blending
  * combines the shaded source with the framebuffer destination, then the
  * write mask decides which RGB channels commit. The rendered examples below
  * show the same source rectangle with all channels enabled, with only green
  * writes enabled, and with constant-alpha source-over-destination blending.
  * In the green-mask image, the untouched red/blue destination channels are
  * what make the result dark and muted; in the blend image, the destination
  * still contributes because the pass constant alpha is below 1.
  *
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="rasterizer_color_output.js"></script>
  * @endhtmlonly
  *
  * <table><tr>
  * <td>@image html rasterizer_color_output_rgb.png "RGB write mask"</td>
  * <td>@image html rasterizer_color_output_green_mask.png "green-only write mask"</td>
  * <td>@image html rasterizer_color_output_constant_alpha.png "constant-alpha blending"</td>
  * </tr></table>
  *
  * Viewport and scissor state are framebuffer-space pass controls. The
  * viewport maps clip-space coordinates into a sub-rectangle of the render
  * buffer; the scissor rectangle rejects fragments outside its bounds after
  * projection. Both rectangles are clipped to the framebuffer at render time.
  * The viewport example changes the projection scale, so the same clip-space
  * square becomes a smaller image. The scissor example keeps the original
  * projection and simply masks the final fragments to a framebuffer rectangle.
  *
  * <table><tr>
  * <td>@image html rasterizer_viewport_full.png "full framebuffer"</td>
  * <td>@image html rasterizer_viewport_rect.png "viewport rectangle"</td>
  * <td>@image html rasterizer_scissor_rect.png "scissor rectangle"</td>
  * </tr></table>
  *
  * @par Tessellation, attributes, and materials
  *
  * LOD controls how dense the generated mesh is before rasterization starts.
  * The sphere sequence shows the visible side effect: low LOD exposes a coarse
  * faceted silhouette, while higher LOD spends more triangles to approach the
  * smooth analytic sphere shape.
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
  * The checkerboard image is the more practical texture case, while the UV
  * color diagnostic is deliberately artificial: a smooth red/green gradient is
  * easier to audit than a textured picture when looking for interpolation or
  * clipping mistakes.
  *
  * The built-in material preview path handles the local direct-lighting subset
  * shared by the raytracer and rasterizer. The scene below keeps the first two
  * spheres Matte-only so ambient and diffuse coefficients are easy to compare,
  * then uses two Phong spheres to show broad and tight specular lobes.
  * Reflection, refraction, and recursive portal transport are raytracer effects;
  * the rasterizer preview stays on local material terms.
  *
  * @image html rasterizer_material_preview.png "Matte coefficient and Phong specular preview"
  *
  * @par Antialiasing
  *
  * The antialiasing figures separate coverage quality from postprocessing.
  * Raw 1x coverage is binary at each pixel center. FXAA only sees the completed
  * image, so it can soften contrast but cannot recover geometry that never
  * covered a pixel sample. MSAA reruns coverage at several subpixel positions,
  * so the resolve can represent a partly covered pixel directly.
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
  * @par Directional shadow maps
  *
  * Shadow maps add a second raster pass from the light's point of view. That
  * light pass records nearest depth only; the normal camera pass later compares
  * each shaded point against that stored depth. The images show the visible
  * effect first, and the widget then exposes the underlying depth comparison.
  *
  * Directional-light shadow maps are another opt-in quality/performance
  * feature. With them enabled, the rasterizer first renders a depth-only
  * view from each directional light, then the camera pass projects shaded
  * points into that light-space image before adding direct material light.
  *
  * <table><tr>
  * <td>@image html rasterizer_shadow_maps_off.png "Direct material lighting only"</td>
  * <td>@image html rasterizer_shadow_maps_on.png "Directional shadow maps enabled"</td>
  * </tr></table>
  *
  * The widget below separates the two passes: the light pass stores one
  * nearest depth per texel, then the camera pass compares a draggable
  * receiver point against the stored depth plus bias. Move the caster or
  * receiver, lower the shadow-map resolution, and increase the bias to see
  * the same aliasing and shadow-detachment trade-offs exposed by the C++
  * controls.
  * Fragments that project outside their selected shadow-map image are treated
  * as lit; filtering taps outside the map use the same open/light border
  * behavior.
  *
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="rasterizer_shadow_map.js"></script>
  * @endhtmlonly
  *
  * Resolution controls how finely the light-space depth image represents
  * the scene. Low values are fast but visibly quantize shadow edges; higher
  * values cost more raster work and memory.
  * The sequence below keeps the scene and bias fixed so only texel density is
  * changing.
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
  * The split blend controls how much of that partition is linear versus
  * logarithmic: linear splits divide depth evenly, while logarithmic splits
  * spend more cascades near the camera. Each map is fit around its slice in
  * light space, then its center is snapped to the light-space texel grid so
  * small camera movements do not make the shadow projection drift by
  * fractional texels.
  * The first row below changes how many maps are used; the second row keeps
  * four cascades and changes where their depth split boundaries land.
  *
  * <table><tr>
  * <td>@image html rasterizer_shadow_cascades_1.png "1 cascade"</td>
  * <td>@image html rasterizer_shadow_cascades_2.png "2 cascades"</td>
  * <td>@image html rasterizer_shadow_cascades_4.png "4 cascades"</td>
  * </tr></table>
  *
  * <table><tr>
  * <td>@image html rasterizer_shadow_cascade_split_0_00.png "linear split"</td>
  * <td>@image html rasterizer_shadow_cascade_split_0_50.png "practical split"</td>
  * <td>@image html rasterizer_shadow_cascade_split_1_00.png "logarithmic split"</td>
  * </tr></table>
  *
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="rasterizer_shadow_cascades.js"></script>
  * @endhtmlonly
  *
  * Bias is a depth comparison tolerance. Constant bias applies the same
  * light-space depth offset everywhere; slope-scaled bias adds more tolerance
  * where the receiver turns away from the light. Too little bias can let a
  * surface shadow itself due to interpolation and quantization differences;
  * too much bias detaches shadows from their casters.
  * The constant-bias row shows the global trade-off. The slope-bias row keeps
  * the base bias fixed and adds more tolerance only for receiver angles that
  * are difficult for a depth map to represent.
  *
  * <table><tr>
  * <td>@image html rasterizer_shadow_bias_0_030.png "bias=0.030"</td>
  * <td>@image html rasterizer_shadow_bias_0_060.png "bias=0.060"</td>
  * <td>@image html rasterizer_shadow_bias_0_080.png "bias=0.080"</td>
  * <td>@image html rasterizer_shadow_bias_0_250.png "bias=0.250"</td>
  * <td>@image html rasterizer_shadow_bias_1_500.png "bias=1.500"</td>
  * </tr></table>
  *
  * <table><tr>
  * <td>@image html rasterizer_shadow_slope_bias_0_000.png "slope bias=0.000"</td>
  * <td>@image html rasterizer_shadow_slope_bias_0_005.png "slope bias=0.005"</td>
  * <td>@image html rasterizer_shadow_slope_bias_0_020.png "slope bias=0.020"</td>
  * <td>@image html rasterizer_shadow_slope_bias_0_050.png "slope bias=0.050"</td>
  * <td>@image html rasterizer_shadow_slope_bias_0_200.png "slope bias=0.200"</td>
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
  * In the comparison image, PCF applies a uniform blur everywhere; PCSS varies
  * the apparent softness from the receiver's relation to nearby blockers.
  *
  * <table><tr>
  * <td>@image html rasterizer_shadow_filter_mode_pcf.png "fixed PCF"</td>
  * <td>@image html rasterizer_shadow_filter_mode_pcss.png "blocker-search PCSS"</td>
  * </tr></table>
  *
  * @par Coverage, interpolation, and clipping widgets
  *
  * The last three widgets zoom into the math that every rendered image above
  * depends on. They are diagnostic views of the pipeline's geometry stage:
  * edge functions decide coverage, perspective-correct weights carry
  * attributes across the triangle, and clipping creates replacement vertices
  * before the perspective divide.
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
  * @par Limits and threading
  *
  * The implementation is intentionally a CPU software rasterizer. Its camera
  * support is limited to camera types that can provide closed-form clip-space
  * projection, and its default execution path favors predictable single-tile
  * rendering unless the caller explicitly opts into queued tiles.
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
  * make the streaming single-tile path the predictable default.
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

  enum class BlendFactor {
    Zero,
    One,
    SourceColor,
    OneMinusSourceColor,
    DestinationColor,
    OneMinusDestinationColor,
    ConstantColor,
    OneMinusConstantColor,
    ConstantAlpha,
    OneMinusConstantAlpha
  };

  enum class BlendOp {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max
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

  /**
    * Optional pass outputs populated during `render(Buffer<Colord>&)`.
    *
    * These buffers are borrowed; callers own allocation and lifetime.
    * A non-null buffer is written only when its dimensions match the
    * render target. Mismatched buffers are ignored for that render.
    *
    * The values describe the rasterizer's final passing fragments:
    * depth is the post-depth-test Z value, normal is normalized,
    * primitive/material/face identify the source triangle, and stencil
    * mirrors the pass stencil value after stencil operations. Buffers are
    * cleared at the start of every render using the rasterizer's clear
    * values (`nullptr` for pointer IDs and the largest `std::uint64_t`
    * value for face IDs).
    *
    * With MSAA enabled, a pixel can have several passing subpixel
    * samples. These diagnostics store the last passing sample processed
    * for that pixel; use 1x rendering when inspecting exact per-pixel
    * pass state.
    *
    * `cloneForRender()` does not copy these borrowed pointers. GUI code
    * that renders isolated snapshots should attach outputs to the snapshot
    * it actually renders, or wait for the broader render-pass resource
    * contract.
    */
  struct DiagnosticOutputBuffers {
    Buffer<double>* depth = nullptr;
    Buffer<Vector3d>* normal = nullptr;
    Buffer<const render::Primitive*>* primitive = nullptr;
    Buffer<const render::Material*>* material = nullptr;
    Buffer<std::uint64_t>* face = nullptr;
    Buffer<std::uint8_t>* stencil = nullptr;
  };

  using VertexShader = std::function<VertexOutput(const VertexInput&)>;
  using FragmentShader = std::function<Colord(const FragmentInput&)>;

  static constexpr std::uint8_t ColorWriteRed = 0x1;
  static constexpr std::uint8_t ColorWriteGreen = 0x2;
  static constexpr std::uint8_t ColorWriteBlue = 0x4;
  static constexpr std::uint8_t ColorWriteAll =
    ColorWriteRed | ColorWriteGreen | ColorWriteBlue;

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
    * material fragment path. Custom fragment shaders are responsible for their
    * own visibility model and bypass this feature.
    *
    * Shadow-map lookups use an open border: fragments or filter samples that
    * project outside the selected map are treated as lit.
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
    * shadow map per slice. `setShadowCascadeSplitLambda()` controls how
    * those depth ranges are distributed.
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

  /// Returns the practical split blend used for directional shadow cascades.
  /// 0 is linear, 1 is logarithmic, and the default is 0.5.
  inline double shadowCascadeSplitLambda() const { return m_shadowCascadeSplitLambda; }

  /**
    * Sets the linear/logarithmic blend for camera-depth cascade splits.
    *
    * Values are clamped to [0, 1]. A value of 0 preserves uniform linear
    * splits across camera depth. A value of 1 uses logarithmic splits that
    * concentrate more cascade resolution near the camera. The default 0.5
    * is the common practical split used by parallel-split shadow maps.
    *
    * <table><tr>
    * <td>@image html rasterizer_shadow_cascade_split_0_00.png "0.00"</td>
    * <td>@image html rasterizer_shadow_cascade_split_0_50.png "0.50"</td>
    * <td>@image html rasterizer_shadow_cascade_split_1_00.png "1.00"</td>
    * </tr></table>
    */
  inline void setShadowCascadeSplitLambda(double lambda) {
    m_shadowCascadeSplitLambda = std::isfinite(lambda) ? std::clamp(lambda, 0.0, 1.0) : 0.0;
  }

  /// Returns the constant light-space depth bias used by the shadow-map
  /// comparison.
  inline double shadowBias() const { return m_shadowBias; }

  /**
    * Sets the constant light-space depth bias added during the shadow-map
    * comparison to avoid self-shadowing from small interpolation differences.
    * Use `setShadowSlopeBias()` for additional angle-dependent tolerance on
    * receivers that are nearly parallel to the light.
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

  /// Returns the slope-scaled shadow-map bias coefficient. The coefficient is
  /// multiplied by the receiver's clamped light-space slope and added to
  /// `shadowBias()` for each shadow comparison.
  inline double shadowSlopeBias() const { return m_shadowSlopeBias; }

  /**
    * Sets the slope-scaled shadow-map bias coefficient.
    *
    * Slope bias adds little extra tolerance on surfaces facing the light and
    * progressively more tolerance at grazing angles, where one shadow-map
    * texel spans more receiver depth. Values are clamped to at least 0.
    *
    * <table><tr>
    * <td>@image html rasterizer_shadow_slope_bias_0_000.png "0.000"</td>
    * <td>@image html rasterizer_shadow_slope_bias_0_005.png "0.005"</td>
    * <td>@image html rasterizer_shadow_slope_bias_0_020.png "0.020"</td>
    * <td>@image html rasterizer_shadow_slope_bias_0_050.png "0.050"</td>
    * <td>@image html rasterizer_shadow_slope_bias_0_200.png "0.200"</td>
    * </tr></table>
    */
  inline void setShadowSlopeBias(double bias) { m_shadowSlopeBias = std::max(0.0, bias); }

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

  /// Returns whether the rasterizer maps clip-space coordinates into an
  /// explicit framebuffer viewport. Disabled means the full render buffer is
  /// the viewport.
  inline bool viewportEnabled() const { return m_viewportEnabled; }

  /// Returns the configured viewport rectangle. The value may extend beyond the
  /// framebuffer; rendering intersects it with the target buffer before drawing.
  inline const Recti& viewportRect() const { return m_viewportRect; }

  /// Enables an explicit framebuffer viewport. Homogeneous clip coordinates in
  /// the canonical [-1, 1] range map into this rectangle instead of the full
  /// render buffer. Negative widths and heights are clamped to zero.
  void setViewportRect(const Recti& rect);
  inline void setViewportRect(int x, int y, int width, int height) {
    setViewportRect(Recti(x, y, width, height));
  }

  /// Disables the explicit viewport and returns to full-frame projection.
  void clearViewportRect();

  /// Returns whether the framebuffer scissor test is active.
  inline bool scissorTestEnabled() const { return m_scissorTestEnabled; }

  /// Enables or disables the scissor test while preserving the configured
  /// scissor rectangle.
  inline void setScissorTestEnabled(bool enabled) { m_scissorTestEnabled = enabled; }

  /// Returns the configured scissor rectangle. The value may extend beyond the
  /// framebuffer; rendering intersects it with the target buffer before drawing.
  inline const Recti& scissorRect() const { return m_scissorRect; }

  /// Sets and enables the framebuffer scissor rectangle. Fragments outside this
  /// rectangle are discarded after projection but before depth/stencil and color
  /// output. Negative widths and heights are clamped to zero.
  void setScissorRect(const Recti& rect);
  inline void setScissorRect(int x, int y, int width, int height) {
    setScissorRect(Recti(x, y, width, height));
  }

  /// Disables the scissor test and clears its rectangle.
  void clearScissorRect();

  /// Depth comparison applied after coverage and stencil tests.
  /// Defaults to `Less`, matching the original Z-buffer path.
  inline DepthFunc depthFunc() const { return m_depthFunc; }
  inline void setDepthFunc(DepthFunc func) { m_depthFunc = func; }

  /// Signed constant offset added to fragment depth before depth test/write.
  /// Defaults to 0. Positive values push fragments farther away for the default
  /// `DepthFunc::Less` test; negative values pull them forward. Fragment
  /// shaders still receive the un-biased geometric depth.
  inline double depthBias() const { return m_depthBias; }
  inline void setDepthBias(double bias) { m_depthBias = std::isfinite(bias) ? bias : 0.0; }

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

  /// RGB channel mask applied after shading and blending. A disabled channel
  /// preserves the destination framebuffer value while depth/stencil state
  /// still updates normally. Defaults to all channels enabled.
  inline std::uint8_t colorWriteMask() const { return m_colorWriteMask; }
  inline void setColorWriteMask(std::uint8_t mask) { m_colorWriteMask = mask & ColorWriteAll; }
  inline void setColorWriteMask(bool red, bool green, bool blue) {
    setColorWriteMask((red ? ColorWriteRed : 0) | (green ? ColorWriteGreen : 0) |
                      (blue ? ColorWriteBlue : 0));
  }

  /// Fixed-function RGB blending applied before the color write mask. The
  /// current color type has no stored alpha channel; use `ConstantAlpha` /
  /// `OneMinusConstantAlpha` for pass-level alpha-style compositing.
  inline bool blendingEnabled() const { return m_blendingEnabled; }
  inline void setBlendingEnabled(bool enabled) { m_blendingEnabled = enabled; }

  inline BlendFactor sourceBlendFactor() const { return m_sourceBlendFactor; }
  inline BlendFactor destinationBlendFactor() const { return m_destinationBlendFactor; }
  inline void setBlendFactors(BlendFactor source, BlendFactor destination) {
    m_sourceBlendFactor = source;
    m_destinationBlendFactor = destination;
  }

  inline BlendOp blendOp() const { return m_blendOp; }
  inline void setBlendOp(BlendOp op) { m_blendOp = op; }

  inline const Colord& blendConstantColor() const { return m_blendConstantColor; }
  inline double blendConstantAlpha() const { return m_blendConstantAlpha; }
  inline void setBlendConstantColor(const Colord& color) { m_blendConstantColor = color; }
  inline void setBlendConstantAlpha(double alpha) {
    m_blendConstantAlpha = std::isfinite(alpha) ? std::clamp(alpha, 0.0, 1.0) : 1.0;
  }
  inline void setBlendConstant(const Colord& color, double alpha) {
    setBlendConstantColor(color);
    setBlendConstantAlpha(alpha);
  }

  /// Optional programmable vertex stage over already-projected mesh
  /// attributes. Return an adjusted `VertexOutput`, or leave unset
  /// for the built-in pass-through stage.
  inline const VertexShader& vertexShader() const { return m_vertexShader; }
  inline void setVertexShader(VertexShader shader) { m_vertexShader = std::move(shader); }
  inline void clearVertexShader() { m_vertexShader = VertexShader(); }

  /// Optional fragment stage over the perspective-correct interpolated
  /// attributes. Leave unset for the built-in material fragment shading path.
  inline const FragmentShader& fragmentShader() const { return m_fragmentShader; }
  inline void setFragmentShader(FragmentShader shader) { m_fragmentShader = std::move(shader); }
  inline void clearFragmentShader() { m_fragmentShader = FragmentShader(); }

  /// Returns the borrowed diagnostic buffers written by direct renders.
  inline const DiagnosticOutputBuffers& diagnosticOutputBuffers() const {
    return m_diagnosticOutputBuffers;
  }

  /// Attaches borrowed diagnostic buffers. Buffers must outlive the render.
  inline void setDiagnosticOutputBuffers(DiagnosticOutputBuffers buffers) {
    m_diagnosticOutputBuffers = buffers;
  }

  /// Stops writing diagnostic pass outputs.
  inline void clearDiagnosticOutputBuffers() {
    m_diagnosticOutputBuffers = DiagnosticOutputBuffers();
  }

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
  double m_shadowCascadeSplitLambda{0.5};
  double m_shadowBias{1e-3};
  double m_shadowSlopeBias{0.0};
  int m_shadowFilterRadius{0};
  ShadowFilterMode m_shadowFilterMode{ShadowFilterMode::PCF};
  CullMode m_cullMode{CullMode::Both};
  bool m_viewportEnabled{false};
  Recti m_viewportRect;
  bool m_scissorTestEnabled{false};
  Recti m_scissorRect;
  DepthFunc m_depthFunc{DepthFunc::Less};
  double m_depthBias{0.0};
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
  std::uint8_t m_colorWriteMask{ColorWriteAll};
  bool m_blendingEnabled{false};
  BlendFactor m_sourceBlendFactor{BlendFactor::One};
  BlendFactor m_destinationBlendFactor{BlendFactor::Zero};
  BlendOp m_blendOp{BlendOp::Add};
  Colord m_blendConstantColor{Colord::white()};
  double m_blendConstantAlpha{1.0};
  VertexShader m_vertexShader;
  FragmentShader m_fragmentShader;
  DiagnosticOutputBuffers m_diagnosticOutputBuffers;
};

}  // namespace engine::raster
