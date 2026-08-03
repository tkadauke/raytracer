# OpenGL GPU residency plan - May 2026

> **Scope:** make OpenGL textures and renderbuffers persist across compatible
> GPU passes in a render graph, so multi-pass GPU plans stop round-tripping
> every intermediate resource through CPU buffers. Unlocks both
> `AttachmentLoadOp::Load` on the OpenGL backend and the "no readback
> between every pass" acceptance criterion from
> `docs/plans/complete/opengl-gpu-rasterizer.md` Phase 3.
>
> **Status:** active and partially implemented. Prerequisite work —
> graph-side resource-domain metadata, GPU residency metadata in
> `RenderResourceStorage`, GPU descriptor surface in trace exports, explicit
> `RenderPassKind::Readback` nodes, attachment store/discard state, color
> attachment Load from a CPU source buffer, the OpenGL raster resource cache,
> and typed graph-owned `OpenGLRasterResource` storage are already in place.
> What is still missing is the end-to-end pass execution path: OpenGL raster
> passes do not yet publish FBO attachments as graph resident resources, later
> passes do not consume those handles, graph pass-state application still
> rejects OpenGL attachment Load, and `OpenGLRasterizer` still rejects
> depth/stencil Load until residency can seed attachments from prior GPU passes.
>
> **Related plans:** `docs/plans/complete/opengl-gpu-rasterizer.md` owns the
> rasterizer pipeline; this plan handles the resource side.
> `docs/plans/render-graph.md` owns pass-compatibility metadata and
> resource validation.

---

## Why

Today every OpenGL raster pass eagerly materializes its color/depth/stencil
outputs into CPU `Buffer<T>` objects via `copyColorTo` / `copyDepthTo` /
`copyStencilTo`, and every consumer pass either reads CPU or fails on a
descriptor-only-GPU input. Multi-pass GPU plans pay an FBO→CPU readback +
CPU→FBO upload round-trip per pass even when the producer and consumer both
live on the GPU. The Phase 4 caching work made each pass faster, but the
inter-pass bandwidth bottleneck is what limits throughput on real plans.

The graph already knows when a pass chain is GPU-compatible (resource-domain
validation + per-pass supported-domain metadata exist), so the missing piece
is purely a backend resource type that the OpenGL executor produces and
consumes.

## Non-goals

- **Not** a portable GPU-resource abstraction. This adds an OpenGL-specific
  backend; future Vulkan/Metal residency, if it ever happens, would
  introduce its own resource type and the storage layer would discriminate.
- **Not** automatic readback elision. Resources still need an explicit
  `RenderPassKind::Readback` node to cross to CPU; the optimization here is
  only that GPU producers and GPU consumers can talk to each other
  directly.
- **Not** cross-rasterizer sharing. Resources are scoped to one
  `OpenGLRasterResourceCache` (one `OpenGLRasterizer` instance, one
  thread). Cross-context share groups are a separate concern.

## Architecture

The shape mirrors the existing CPU resource path:

```text
RenderPlan
  -> RenderResourceStorage (per-render scratch)
      .allocate(plan.resources())     // CPU: Buffer<T>; OpenGL: this plan
      .bind(resource, OpenGL handle)  // when producer is the OpenGL backend
  -> pass executors
      OpenGLRasterizer
        producer: writes into FBO attachment → registers GL handle on resource
        consumer: reads GL texture handle from resource as input attachment
      Other executors
        consumer: see descriptor-only GPU input → fail clearly OR a
                  scheduled readback node materializes CPU first
```

Add an `engine::raster::OpenGLRasterResource` struct (or similar) that
holds:

* a `GLuint` texture handle (the resident GPU-side storage),
* the `RenderResourceKind` it satisfies (color, depth, stencil, normal,
  worldpos, …),
* the `QOpenGLContext*` it lives in (a sanity tag — consumers in a
  different context must fall through to the readback path).

`RenderResourceStorage` learns to hold either a CPU `Buffer<T>` or an
OpenGL handle per resource id, keyed on the resource's declared backend
domain. The validation that already rejects CPU passes reading GPU
descriptors does not change; it just gets a real GPU side to validate
against.

## Phase 0 — backend resource type and storage hook

Tasks:

- ~~Define `OpenGLRasterResource` in `include/engine/raster/detail/` holding
  a texture handle, kind, source rasterizer context pointer, and a
  destructor that releases the handle with the source context current
  (matching the existing `OpenGLRasterResourceCache` lifecycle pattern).~~ ✅
  **Done.** `include/engine/raster/detail/OpenGLRasterResource.h` owns typed
  texture/renderbuffer handles and releases them through the source context,
  with lifecycle coverage in `OpenGLRasterResourceTest.cpp`.
- ~~Extend `RenderResourceStorage` to accept and surface OpenGL resources
  through a typed accessor (`storage.openGLResourceFor(id)` returning
  `OpenGLRasterResource*` or null).~~ ✅ **Done.** `RenderResourceStorage` now
  binds, clears, validates, and retrieves `OpenGLRasterResource` handles for
  GPU-domain descriptors, with shape/type/access checks covered in
  `RenderResourceStorageTest.cpp`.
- Add a graph-level acceptance test that asserts an OpenGL-only pass chain
  serializes its inter-pass resources as OpenGL handles, not CPU buffers. This
  remains TODO because no OpenGL pass currently produces a resident attachment
  for a downstream graph consumer.

## Phase 1 — `OpenGLRasterizer` produces resident outputs

Tasks:

- Teach `OpenGLRasterizer` to publish its FBO color attachment as an
  `OpenGLRasterResource` on the storage when a downstream consumer pass
  declares OpenGL-domain input. Skip the `copyColorTo` readback for that
  case.
- Same for depth attachment and the stencil-attached path.
- The existing per-render CPU-readback path stays as the fallback when
  the consumer is CPU-only or when an explicit `Readback` node is
  scheduled.
- Parity test variant: two OpenGL raster passes in a plan, second reads
  the first's color as albedo input; total pixels still match the CPU
  reference within the parity tolerance.

## Phase 2 — `OpenGLRasterizer` consumes resident inputs

Tasks:

- ~~Implement `AttachmentLoadOp::Load` for color, depth, and stencil:
  blit the GL handle from the input resource into the FBO attachment
  at pass start instead of clearing.~~ ⏳ **Partially done.** Color
  Load works via CPU-buffer round-trip: the caller provides a
  `Buffer<Colord>` through `setColorLoadSource`; the rasterizer
  uploads it via a temporary `GL_RGBA32F` texture + `glBlitFramebuffer`
  into the AttachmentSet's color renderbuffer at pass start, then
  masks the color clear bit. Depth and stencil Load still throw with
  narrower messages naming the missing slice. True GPU-resident Load
  (no CPU round-trip when both producer and consumer are GPU-domain)
  needs the `OpenGLRasterResource` substrate from Phase 0/1.
- ~~Make a small parity test: a "preserve depth across two GPU passes"
  scene where the second pass's `depthLoadOp = Load` and the first
  pass's depth must show through.~~ ⏳ **Blocked on depth Load.**
- ~~Remove the three `does not support … attachment load yet` throws
  in `OpenGLRasterizer.cpp`.~~ ✅ **Done.** The original generic
  throws are gone. Two narrower throws remain — "Load requires a
  source buffer" (the contract check; fires when the caller misuses
  the API) and "depth/stencil Load not yet implemented" (narrower
  message naming the missing slice). The five tests in
  `OpenGLRasterizerTest.cpp` cover both new throw paths plus the
  color Load success path. Commit: see the corresponding
  rasterizer change.

## Phase 3 — graph-driven scheduling polish

Tasks:

- When the graph compiles a pass chain, mark the intermediate resources
  with their preferred domain (`OpenGL` if all producers and consumers
  support it). ✅ **Partially done.** Graph resource descriptors already carry
  CPU/GPU domains and pass nodes declare supported domains; concrete
  OpenGL-only pass-chain scheduling still awaits the typed resident resource
  from Phase 0/1.
- Trace messages distinguish "kept resident on GPU" from "round-tripped
  via readback" so a user inspecting a plan can see the optimization
  paid off. ⏳ **Partially done.** Trace snapshots include GPU residency
  metadata when a resource has it, but no OpenGL pass currently produces the
  resident handle that would make the optimization visible end-to-end.
- rendercli functional test: a multi-pass GPU plan reports zero implicit
  readback nodes between compatible passes.

## Out of scope (deferred)

- **Sharing GL resources across rasterizer instances.** Each clone owns
  its own context; cross-context share groups would let resources travel
  between threads but require careful Qt setup. Tracked as a future
  follow-up if the parallel render tile model ever moves to GPU.
- **Mipmap regeneration on GPU outputs.** First version produces flat
  level-0 textures; consumers that want mipmaps trigger CPU readback for
  now.
- **Automatic readback insertion at executor boundaries.** The graph
  compiler already inserts `Readback` nodes where needed; backend-side
  auto-insertion is unnecessary and would hide intent from the trace.

## How to verify

- New unit tests cover storage type discrimination and resource
  destructor lifecycle.
- New parity test variants cover producer→consumer GPU chains.
- Existing OpenGL parity tests stay green (single-pass behavior is
  unchanged).
- rendercli trace inspection shows the load/store transitions.
