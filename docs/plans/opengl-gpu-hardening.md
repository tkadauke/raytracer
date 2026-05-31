# OpenGL GPU hardening + Qt decoupling plan - May 2026

> **Scope:** clean up the OpenGL raster backend so the remaining
> `opengl-gpu-rasterizer.md` and `opengl-gpu-residency.md` work has a
> sound foundation, AND remove the Qt-OpenGL dependency from the engine
> library so `rendercli` stops dragging in `QGuiApplication`.
>
> **Status:** planning. The OpenGL backend works end-to-end and ships in
> rendercli/Modeler today; this plan covers technical-debt cleanup,
> robustness fixes, and the Qt → native-GL migration that the open
> questions in `opengl-gpu-rasterizer.md` and `opengl-gpu-residency.md`
> have been waiting on.
>
> **Related plans:** `docs/plans/opengl-gpu-rasterizer.md` (pipeline),
> `docs/plans/opengl-gpu-residency.md` (resource lifetime),
> `docs/plans/render-graph.md` (resource validation).

---

## Why

Five recent commits (Y-flip, FitExact, mesh-cache cache-key fix,
process-exit crash, camera-independent mesh build) revealed structural
issues in the OpenGL backend that residency + remaining attachment-load
work will magnify:

- `OpenGLRasterDrawPass` has a 33-parameter constructor. Adding state
  requires touching the constructor, call sites, init list, members,
  and `cloneForRender`. Residency adds at least three more (attachment
  set, load/store policy per attachment, GPU-residency flag).
- `OpenGLOffscreenContext` owns exactly one FBO. Residency needs
  multiple per-resource attachments living across passes; the
  single-FBO model can't be incrementally extended.
- The mesh cache stores a single entry; multi-pass graphs (color +
  depth AOV + stencil) thrash. Cache keys hold raw pointers for
  `visibilitySet` / `shadowMaps` (same `weak_ptr` lesson we already
  applied to `scene`).
- `rendercli` boots `QGuiApplication` and forces `QT_QPA_PLATFORM=offscreen`
  just to create an offscreen GL context. The library couples the GL
  backend to Qt's OpenGL classes (~102 Qt symbol references across
  three files). The Modeler doesn't actually use Qt-side GL
  integration — it renders offscreen and paints the readback as a
  `QImage`, exactly like rendercli.
- All GL tests `GTEST_SKIP` when no offscreen GL context is available.
  CI without GL is silent. There is no failing parity test waiting for
  attachment-load or residency to land.

The remaining `opengl-gpu-rasterizer.md` and `opengl-gpu-residency.md`
items either get harder under the current shape (state struct,
attachment set, cache scaling) or assume infrastructure that isn't
there yet (CI, ground-truth fixtures).

## Non-goals

- Not a rewrite of the GL rasterizer. The shader, mesh layout,
  attribute bindings, and pipeline stay.
- Not switching graphics APIs (Vulkan, Metal). Modeler stays GL.
- Not a window-system / input library introduction (GLFW, SDL). The
  backend is offscreen-only; per-platform context bring-up is narrow.
- Not removing Qt from the Modeler. The Modeler keeps Qt for UI; only
  the GL backend stops depending on it.

## Architecture

Three layered changes, sequencing matters:

```text
1. internal cleanup (no API change, no Qt change)
   - state-struct draw pass
   - cached vertex-buffer upload skip
   - multi-entry mesh cache
   - asymmetric ground-truth parity fixtures
   - centralized attachment-load support gate

2. Qt → native-GL decoupling
   - engine::raster::gl::{Context, Buffer, ShaderProgram, Framebuffer}
   - per-platform Context impls (CGL, EGL)
   - rasterizer/cache use the wrappers
   - rendercli drops QGuiApplication

3. residency-ready substrate
   - OpenGLAttachmentSet replaces the single FBO
   - per-resource texture residency
   - attachment-load implementations follow
```

Phase 1 unblocks both subsequent phases. Phases 2 and 3 are mostly
independent and can be reordered if residency turns out to be the
higher-priority item.

## Phase 1 — internal cleanup (no Qt change) ✅ **Done.**

Goals: make the codebase ready for both decoupling and residency.
Land in small commits; each is a refactor with no behavior change.

Tasks:

- ~~**`OpenGLRasterDrawState` struct.** Bundle the 33
  `OpenGLRasterDrawPass` constructor args into a state struct.~~ ✅
  **Done.** Commit `bfa34979` — state struct replaces the 33-arg
  ctor; the struct moved into
  `include/engine/raster/detail/OpenGLRasterDrawState.h` in `7615ec26`
  to make it reusable by the fixed-function helpers.
- ~~**Skip vertex-buffer re-upload on mesh-cache hit.**~~ ✅ **Done.**
  Commit `71f8c9ed` — `OpenGLRasterResourceCache::cachedMeshUploaded`
  tracks GPU-buffer freshness; subsequent commit `00aaa5c3` upgraded
  the flag to `uploadedMeshSlot` for the multi-entry LRU.
- ~~**Multi-entry mesh cache (LRU n=4).**~~ ✅ **Done.** Commit
  `00aaa5c3` — fixed-size `meshCache[4]` with monotonic
  `meshUseTick` for LRU eviction, `uploadedMeshSlot` for upload-skip
  tracking; new `LruRetainsEntriesAcrossMultiPassThrashing` regression
  test.
- ~~**`weak_ptr` for `visibilitySet` and `shadowMaps` in
  `OpenGLMeshCacheKey`.**~~ ✅ **Done.** Commit `277c3370` — both
  fields now `weak_ptr<const T>`; `matches()` compares via
  `lock().get()`.
- ~~**Centralize attachment-load support check.**~~ ✅ **Done.**
  Commit `20e09e21` — single pre-bind check in `renderOpenGL`; the
  three scattered throws are gone; new
  `OpenGLRasterizerAttachmentLoad` test fixture (commit `f7b89d3f`)
  pins the rejection contract pending Phase 3 residency.
- ~~**Split `OpenGLRasterDrawPass`** into focused detail classes.~~
  ✅ **Partial.** Commit `7615ec26` extracted the fixed-function
  state group into `engine/raster/detail/OpenGLFixedFunctionState.h`
  (apply* helpers + enum converters). The attribute-binder,
  lighting-uniforms, and draw-loop splits are deferred — they're
  entangled with the resource cache and shader program in ways the
  fixed-function group wasn't. Revisit during Phase 3 when the
  attachment-set carve-out forces the issue.
- ~~**Promote `OpenGLFallbackTexture` / `OpenGLShadowTexture`** to
  their own header.~~ ✅ **Done.** Commit `2301c7b0` — both classes
  now live in `include/engine/raster/detail/OpenGLRasterTextures.h`
  and are reusable for Phase 3 per-attachment-set wrappers.
- ~~**Asymmetric ground-truth parity fixtures.**~~ ✅ **Done.**
  Commit `2f63ab84` — three TEST_F entries in
  `OpenGLRasterizerParityTest.cpp` place colored spheres off-axis and
  assert the rendered region matches the project's screen convention.
  Verified by reverting the Y-flip — the new GPU test fails, the CPU
  reference stays green.

Acceptance:

- ✅ All existing tests still pass (94 GL tests, 3789/3791 overall;
  the two failures are pre-existing GltfSceneImporter/Molecule cases
  unrelated to GL).
- ✅ `OpenGLRasterDrawPass` constructor now takes 3 parameters
  (resources, state struct, cancellation flag).
- ✅ Multi-pass mesh cache verified by
  `LruRetainsEntriesAcrossMultiPassThrashing`.
- ✅ Failing-pending-residency tests exist
  (`OpenGLRasterizerAttachmentLoad.{Color,Depth,Stencil}LoadOp...`).

## Phase 2 — Qt → native-GL decoupling ✅ **Done.**

Goals: remove Qt OpenGL classes from the engine library. Modeler keeps
Qt for UI; rendercli drops `QGuiApplication`. ~600-800 lines of
mechanical wrapper code + 200-300 lines of per-platform context bring-up.

**Landed:**

- `engine::raster::gl::Context` abstract interface (`d5f7d73a`).
- `OpenGLOffscreenContext` implements `gl::Context` so the Qt-backed
  impl satisfies the contract (`c4a605bc`).
- `gl::CGLContext` native backend for macOS — self-contained,
  standalone tests cover probe/create/clear+readback round-trip/
  thread cycle (`875263b7`).
- `gl::createOffscreenContext()` factory + `OpenGLRasterResourceCache`
  holds `unique_ptr<gl::Context>` (`99349a96`).
- `gl::Buffer` RAII wrapper over `glGenBuffers`/`glBufferData`;
  rasterizer VBO/IBO migrated off `QOpenGLBuffer` (`51e83c7f`).
- `gl::ShaderProgram` RAII wrapper over `glCreateProgram` +
  uniform/attrib helpers; rasterizer migrated off
  `QOpenGLShaderProgram` (`bd9f9237`).
- `QOpenGLContext::currentContext()->functions()->glX(…)` lookups
  replaced with raw `glX(…)` everywhere in the engine library
  (`67aa7c45`). The `<OpenGL/gl.h>` / `<GL/gl.h>` shim in
  `gl/Bindings.h` is the only header the rasterizer needs for
  function symbols.
- `RenderCliApplication` constructs `QCoreApplication`
  unconditionally; the factory selects CGL automatically when no
  `QGuiApplication` is up.
- `gl::EglContext` native backend for Linux using Mesa's surfaceless
  EGL platform. End-to-end render verified in the Ubuntu 24.04
  Docker container with `LIBGL_ALWAYS_SOFTWARE=1`; 4 standalone
  EglContext tests cover probe/create/clear+readback/thread cycle.

**Deferred to Phase 3:**

- `gl::Framebuffer` wrapper. The FBO is owned by the context backend
  (Qt, CGL, EGL each manage their own); a shared wrapper pays off
  when attachment-set carve-out happens.

New directory: `include/engine/raster/gl/`

```text
gl/Context.h          // abstract: makeCurrent/doneCurrent/threading  ✅
gl/CGLContext.h       // macOS Core OpenGL                            ✅
gl/ContextEGL.h       // Linux/headless EGL (Mesa or SwiftShader)
gl/Buffer.h           // RAII over glGenBuffers/glBufferData
gl/ShaderProgram.h    // RAII over glCreateProgram + uniform/attrib helpers
gl/Framebuffer.h      // RAII over glGenFramebuffers + MSAA resolve via blit
gl/Loader.h           // function-pointer table (glad or hand-rolled)
```

Tasks:

- **Define `engine::raster::gl::Context`** abstract interface (create
  offscreen with width/height/samples, make-current, done-current,
  detach/migrate thread, get function loader). Modeled on the current
  `OpenGLOffscreenContext::Private` interface; Qt details drop out.
- **`Buffer` / `ShaderProgram` / `Framebuffer` wrappers** over raw GL
  with the same surface as the Qt classes we use today. Same
  signatures the rasterizer already calls means a near-zero migration
  diff.
- **Function-pointer loader.** Either vendor `glad` (single header, no
  runtime cost, ~5 KB) or hand-roll `dlsym` / `CGLGetProcAddress` for
  the ~80 GL entry points we use. Decision in the open questions
  below.
- **`ContextCGL` (macOS).** ~200 lines using `<OpenGL/CGL.h>`. Creates
  a pbuffer-backed offscreen context, handles thread affinity via
  `CGLSetCurrentContext`. Modern Cocoa-GL deprecation warnings stay
  contained here.
- **`ContextEGL` (Linux/headless).** ~150 lines. Uses Mesa EGL's
  surfaceless extension; no X11 needed. Also the path SwiftShader
  takes for CI.
- **Migrate `OpenGLOffscreenContext`** to a thin facade over a
  `gl::Context` instance. Public surface unchanged; only the impl
  flips. Qt-OpenGL includes disappear from the .cpp.
- **Migrate `OpenGLRasterResourceCache` and `OpenGLRasterizer`** to
  the new `gl::` wrappers. Mostly `s/QOpenGLBuffer/gl::Buffer/`. The
  per-draw uniform calls stay in the rasterizer; the program API just
  changes signatures.
- **Drop `QGuiApplication` from rendercli.** `RenderCliApplication`
  collapses to constructing `QCoreApplication` always; the
  `QT_QPA_PLATFORM=offscreen` dance goes away.

Acceptance:

- `git grep -l QOpenGL include/engine src/engine` returns no files.
- `rendercli --raster_backend gpu` runs without `QGuiApplication` (CPU
  + GPU paths share one `QCoreApplication`).
- Modeler still uses the same `engine::raster::gl::Context` (no Qt
  GL backend needed; Modeler doesn't integrate Qt-side anyway).
- Parity tests run under both macOS CGL and Linux EGL backends
  (selectable at build time via `RAYTRACER_GL_BACKEND=cgl|egl|auto`).

## Phase 3 — residency-ready substrate

Goals: prepare for `opengl-gpu-residency.md` Phase 0. Most of the
work is the attachment-set carve-out; residency itself is then a
short follow-up.

Tasks:

- **`OpenGLAttachmentSet` (color + depth + stencil triple)** lives on
  the resource cache, replaces the single bound FBO. Addressable by
  graph-resource id; one set per active resource group.
- **`OpenGLOffscreenContext` stops owning attachments.** It becomes
  pure context lifecycle.
- **Per-render attachment-set selection.** The draw state struct
  carries the attachment-set handle to use; the rasterizer binds it
  before issuing draws.
- **Trace messages name the attachment set + load/store ops** so a
  Modeler/rendercli trace inspector can see when a pass keeps an
  attachment resident vs allocates fresh.

After this, `opengl-gpu-residency.md` Phase 0-2 work is mostly
"register the attachment-set's texture with the graph storage" — the
plumbing exists, residency only adds the lifetime contract.

## Test / CI

Required across all three phases:

- **GL parity tests** stay green under both `cgl` (macOS) and `egl`
  (Linux/CI) backends. Existing `GTEST_SKIP` gate moves from "any GL"
  to "no GL backend selected at build time."
- **Asymmetric ground-truth parity fixtures** land in Phase 1. Each
  fixture is a PNG checked in alongside the test scene; CPU rasterizer
  is the reference. CI compares both backends to the fixture, not to
  each other.
- **Linux EGL CI lane** lands in Phase 2 as soon as the EGL backend
  exists. Uses Mesa EGL with `LIBGL_ALWAYS_SOFTWARE=1` or SwiftShader
  for deterministic output. This is the answer to the open question in
  `opengl-gpu-rasterizer.md`.
- **Attachment-load failing test** lands in Phase 1, skipped pending
  Phase 3. Gives residency a clear acceptance signal.
- **Multi-pass cache-thrashing test** lands in Phase 1. Confirms the
  LRU mesh cache works.

## Open questions

- **`glad` vs hand-rolled GL loader.** `glad` is a vendored header,
  zero runtime cost, well-tested. Hand-rolling is ~50 lines and one
  fewer dependency. Lean toward `glad` because it covers ES + desktop
  + extensions out of the box (useful if we add ANGLE later); hand-
  rolled covers exactly what we need today and nothing more.
- **macOS Cocoa-GL deprecation hedge.** Apple still ships Cocoa GL on
  macOS 15 with loud warnings; removal is plausibly within 2-3 OS
  versions. **ANGLE** (OpenGL ES over Metal) is the standard hedge;
  it adds a ~30 MB build dep but works on macOS, Windows, and as a
  CI fallback. The abstraction layer makes it a fourth backend
  (`ContextANGLE.h`) rather than a rewrite. Open: ship the hedge in
  Phase 2 or defer until Cocoa GL is actually deprecated?
- **Modern core profile vs GL 2.1.** We currently target 2.1 to keep
  Qt's OpenGL classes happy on broad hardware. CGL/EGL can request
  a 3.3+ core profile and we'd retire the legacy `attribute` /
  `varying` shader syntax. Worth doing during Phase 2 or as a
  separate later cleanup?
- **Multi-mesh cache eviction policy.** LRU n=4 fits typical graphs
  (color, depth AOV, stencil AOV, object ID). Open whether to size
  dynamically based on graph-pass count or stay fixed.
