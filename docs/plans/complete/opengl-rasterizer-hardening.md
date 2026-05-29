# OpenGL rasterizer hardening plan - May 2026

> **Scope:** correctness, parity, and pipeline-cost fixes for the
> `engine::raster::OpenGLRasterizer` backend shipped by the OpenGL GPU
> rasterizer plan. The backend executes through the render graph; this plan
> tightens it to the point where a graph-driven render produces a CPU-faithful
> image at a cost that is at least competitive with the software path.
>
> **Status:** planning. Builds on `docs/plans/opengl-gpu-rasterizer.md`. The
> phase boundaries here are commit boundaries — each phase is one self-contained
> change that builds, ships a test where applicable, and updates the changelog.
>
> **Related plans:** `docs/plans/opengl-gpu-rasterizer.md` owns backend scope
> and graph integration. `docs/plans/rasterizer-performance.md` tracks raster
> performance diagnostics. `docs/plans/cpu-culling-graph-pass.md` is unrelated
> and proceeds independently.
>
> **Out of scope here:** anything that requires extending the graph compiler
> or pass-state surface; portals/mirrors/recursive views; share-group threading
> for `cloneForRender` (called out at the bottom but not implemented).

---

## Why

A pass over the current `OpenGLRasterizer` surfaced a mix of correctness gaps
and per-frame waste:

- Cull mode, depth bias, and HDR texture upload do not match what the public
  API says they do.
- Excess lights are silently dropped at a hard-coded cap of 8 per kind.
- Cancellation only fires during CPU mesh build; the GL draw runs to
  completion afterward.
- The shader program, GPU buffers, image textures, and offscreen context are
  all rebuilt on every `render()` call.
- The vertex format carries per-batch material/light data per vertex.
- GLSL is embedded as a multi-page C++ string literal.
- There is no GPU-vs-CPU parity test; the existing tests check field
  pass-through and a single offscreen-context readback, not pixels.

Each item below is one commit. Order is chosen so that the parity test lands
first (it catches every later regression), the shader source moves into its
own files before shader edits get noisy, and caching arrives once correctness
is pinned.

## Phase 1 — parity test scaffolding

### Commit 1: CPU-vs-OpenGL parity smoke test

Add `test/unit/engine/raster/OpenGLRasterizerParityTest.cpp` that builds a
tiny fixture scene (one sphere, one directional light, a plain matte material,
default camera), renders it with both `Rasterizer` and `OpenGLRasterizer` at a
small resolution (e.g. 64×64), and compares the resulting `Buffer<Colord>`
pixel-by-pixel against a tolerance that accommodates floating-point and
fixed-function differences (~5/255 per channel for the lit areas).

Skip the test gracefully when `OpenGLOffscreenContext::probe()` reports
unavailable, matching the existing offscreen test pattern. This canary is
load-bearing for every following commit.

## Phase 2 — correctness fixes pinned by the parity test

### Commit 2: honor cull mode via `glCullFace` / `glEnable(GL_CULL_FACE)`

`OpenGLRasterizer` already plumbs cull mode into `OpenGLRasterMeshBuilder`,
which filters CPU-side. The GL pipeline never enables face culling. Either
(a) drop the CPU filter and use only `glCullFace` for the override, or (b)
keep CPU filter and document the choice with the header docstring. Pick (a)
for consistency — the GL fixed-function path is free and the mesh builder
should not double-decide. The parity test gets a back-culled variant to pin
that visible behavior actually changes.

### Commit 3: depth bias via `glPolygonOffset` — skipped

Investigation: CPU `Rasterizer` treats `depthBias` as a constant additive
offset in NDC depth space (`DepthState::biasedDepth` = `depth + bias`). The
OpenGL backend bakes the same constant offset into per-vertex NDC depth in
`OpenGLRasterMesh.cpp`, matching CPU semantics. Switching to
`glPolygonOffset` would apply a slope-scaled, depth-buffer-precision-dependent
bias instead — that is a different contract, not a correctness fix. Revisit
only if/when the public `depthBias` is redefined as slope-aware.

### Commit 4: stop clamping image textures at upload

`OpenGLTextureCache::texturePixels` clamps each channel to [0, 1] before
uploading to `GL_RGBA32F`. Drop the clamp so HDR image textures keep their
range. The fallback texture stays unchanged (it is white by construction).

### Commit 5: emit a diagnostic when light counts are truncated

The shader caps `kMaxDirectionalShaderLights` and `kMaxPointShaderLights` at 8
each, and `setLightingUniforms` silently truncates. Push an
`OpenGL raster shader truncated N directional lights (8 supported)` line into
`m_lastTraceMessages` when the mesh's light list exceeds the cap. (Raising the
cap or moving lights to a UBO is deferred — the trace is the smallest fix that
removes the silent divergence and is testable in the unit suite.)

### Commit 6: avoid the throwaway color buffer in `renderDepth`/`renderStencil`

`OpenGLRasterizer::renderDepth` allocates a `Buffer<Colord>` the size of the
target only to satisfy the `renderOpenGL` signature. Make `renderOpenGL`
accept a nullable color target and gate `copyColorTo` / the color trace on
non-null + `colorStoreOp == Store`.

### Commit 7: cancellation checkpoint inside the draw pass

`OpenGLRasterDrawPass::render` runs to completion once entered. Take a
`std::atomic<bool>&` ref into the draw pass and bail between batches inside
the `for (const auto& batch : mesh.batches())` loop. The initially-planned
extra checkpoints (before context creation, before the FBO bind) were tried
and reverted: they leave the target buffer untouched and so they broke the
Modeler's "drag camera and re-render" flow when callers did not yet
`uncancel()` between renders. The CPU `Rasterizer` deliberately checks
cancellation only inside its inner per-triangle loop (see the comment at
`Rasterizer.cpp:743`, "Caller is expected to call uncancel() between
renders"); the OpenGL backend now matches that contract.

### Commit 8: refresh the stale header docstring + add a CHANGELOG rollup entry

The header at `include/engine/raster/OpenGLRasterizer.h:24-32` still describes
the class as a "shell" rendering "clipped triangles with material albedo,"
which has not been true since the lit-mesh path landed. Rewrite to describe
the actual feature set (Phong-style direct lighting, alpha test, blending,
stencil, optional external shadow textures, image/checker albedo). Add a
single CHANGELOG entry summarizing what the backend renders today (the prior
entries from earlier in `Unreleased` describe individual increments).

## Phase 3 — extract the shader source

### Commit 9: move GLSL into `shaders/opengl_raster_*.{vert,frag}`

The vertex+fragment GLSL inlined in `OpenGLRasterizer.cpp:344-608` is ~250
lines as a C++ string literal. Move both into separate files under
`src/engine/raster/shaders/` (or `include/...` if Qt resource compilation
lives elsewhere — match the project's existing GLSL/resource pattern if there
is one, otherwise compile-time-include via `#include` of a generated header
or use a Qt `qrc` resource). The behavior is byte-identical; the diff is
purely the move plus a loader. Required before any shader edit in Phase 4 is
reviewable.

## Phase 4 — pipeline-cost fixes ✅ shipped

The five-step plan was carried out as part of "implement the rest of the
OpenGL GPU rasterizer plan." All caching now lives on a single
`detail::OpenGLRasterResourceCache` member of `OpenGLRasterizer`:

1. ✅ Split `OpenGLOffscreenContext` into idempotent `ensureContext()` and
   `ensureFramebuffer(w, h, samples)`. Same-dimension renders skip the FBO
   rebuild. (commit `a23c7a8c`)
2. ✅ Context ownership moved onto `OpenGLRasterResourceCache`, owned as a
   `mutable std::unique_ptr` by `OpenGLRasterizer`. (commit `79ed1f0a`)
3. ✅ Cache the linked `QOpenGLShaderProgram` and resolved attribute slot
   indices; first render compiles + links, subsequent renders reuse.
   (commit `79ed1f0a`)
4. ✅ Lift `OpenGLTextureCache` onto the resource cache as
   `OpenGLRasterImageTextureCache`; each `ImageTexture` uploads once per
   rasterizer. (commit `bcdee725`)
5. ✅ Persistent vertex/index `QOpenGLBuffer`s on the resource cache, with
   per-frame `allocate()` re-uploads of the current mesh payload.
   (commit `f5cdd0d6`)

Each `cloneForRender` clone owns its own cache, matching Qt's per-thread
context model. The cache destructor makes the offscreen context current
before releasing program, textures, and buffers so all GL handles are
freed against the right context.

## Out of scope (documented, not done)

- **Offscreen context reuse across calls.** GL contexts are per-thread, and
  `cloneForRender` makes per-thread caching the wrong abstraction without a
  share-group story. Pull this once the threading model is settled.
- **Move per-batch material/light data from vertex attributes to uniforms.**
  Real win, but conflicts with how `OpenGLRasterMesh` packs vertices today;
  needs its own plan if it grows beyond a one-commit refactor.
- **Light array as UBO with raised cap.** Same shape — needs its own
  diagnostic-and-correctness-test pass.
- **`cloneForRender` for parallel renders.** The current implementation lets
  each clone create its own GL context. Needs a separate decision about
  whether the GL backend serializes vs. uses share-groups.
- **Move the shared rasterizer state surface into a struct shared with CPU
  `Rasterizer`.** Worthwhile but cross-cuts both classes; tracked here as a
  pointer.

Each out-of-scope item is intentionally left for a follow-up plan so the
hardening pass stays scoped to one PR-sized batch of changes.

## How to verify

- `cmake --preset release && cmake --build --preset release && ctest --preset release`
  after each commit.
- Parity test stays green under each commit. On hosts without GL it skips
  cleanly; the offscreen-context probe pattern is the canary.
- For commits in Phase 4, add no new assertions about pixel output — caching
  is performance-only and the Phase 1 parity test pins the visible behavior.
