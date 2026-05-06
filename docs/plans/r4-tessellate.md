# R4 plan: `Primitive::tessellate(LOD) → Mesh`

> Roadmap §3.R4. Adds a `Mesh` data type and a virtual `tessellate(int lod)`
> on every `Primitive` so non-raytracing engines (wireframe, software raster,
> OpenGL viewport) and exporters (OBJ, STL, glTF) can consume the geometry.

## Existing scaffolding to reuse

`core/geometry/Mesh` already exists (used by the PLY parser and the
`MeshTriangle` primitives). It has:

- `Mesh::Vertex { Vector3d point; Vector3d normal; }`
- `Mesh::Face = std::vector<int>` (N-gon, indexed)
- `addVertex` / `addFace` builders
- `TriangleIterator` that walks face fans for downstream consumers

**Missing:** UVs. Currently a `Vertex` is `(point, normal)` only.

This is the natural home for the tessellation output. We extend it rather
than introducing a parallel `MeshData` type.

## Phase 1 — interactive design

These decisions shape every per-primitive impl, so they get settled first.

### 1a. UV story on `Mesh::Vertex`

**Decision: UVs always present.** `Vertex` becomes
`{ Vector3d point; Vector3d normal; Vector2d uv; }`. PLY-loaded meshes get
`uv = (0, 0)`; meaningful values land when something actually consumes them
(textured rendering, UV editor). The 16-byte cost per vertex is negligible
next to per-primitive intersection state.

### 1b. The `tessellate()` signature

```cpp
virtual std::shared_ptr<Mesh> tessellate(int lod = 0) const;
```

- `lod` = 0 → "minimum reasonable" tessellation (Sphere = octahedron-ish,
  Torus = 8×8 grid).
- Higher values grow the subdivision count (doubling, or otherwise — the
  exact mapping is documented per primitive).
- Default impl returns an empty `Mesh` plus a trace event so debugging
  "my engine has no geometry" surfaces which primitive failed to override.

**Decision: single integer LOD.** Per-axis LOD (separate `lonSubdivisions`
/ `latSubdivisions`) deferred — single int matches the OpenSubdiv pattern,
keeps the API small, and per-axis can layer on later if any consumer
actually wants it.

### 1c. Where the default lives

`Primitive::tessellate(int)` default in the base class — returns empty
`Mesh` and emits a `state.recordEvent` warning so missing overrides are
loud during a render.

### 1d. Canary: Box

Before farming out per-primitive work, one canary primitive lands
end-to-end so workers have a working reference.

- **Box** picked as the canary: simplest non-trivial geometry, naturally
  UV-able (per-face 0..1), 12 triangles end-of-story.
- Implement `Box::tessellate(int lod)`.
- Add `BoxTessellateTest.cpp` covering: triangle count, vertex positions,
  face-normal correctness, UV layout (per-face 0..1 on each of the 6 faces).
- Land Phase 1 + Box as a single commit; that commit becomes the reference
  for the workers.

### 1e. CSG and Instance

Per roadmap, CSG mesh booleans are a separate epic (libigl / BSP-based
v1). For R4:

- CSG primitives' `tessellate()` returns empty + a trace event referencing
  the roadmap §4.2.a future work.
- `Composite::tessellate()` concatenates children's meshes.
- `Instance::tessellate()` tessellates its wrapped primitive and applies
  the transform matrix to every vertex (point via `m_pointMatrix`,
  normal via `m_normalMatrix`).

## Phase 2 — per-primitive implementations (delegate to background workers)

Each is independent once Phase 1 lands. Batched into 3 worker briefings:

| Primitive | Strategy | Difficulty | Batch |
|---|---|---|---|
| Box | 6 faces × 2 triangles | trivial (canary) | done in Phase 1 |
| Plane | infinite — empty mesh + trace | trivial | A |
| Rectangle | 2 triangles | trivial | A |
| Triangle | single triangle, identity | trivial | A |
| `*MeshTriangle` | single triangle, reference parent | trivial | A |
| CSG (5 classes) | empty + trace event | trivial | A |
| Disk | triangle fan, `lod` → segment count | easy | B |
| OpenCylinder | strip, `lod` → segment count | easy | B |
| Composite | concatenate children's meshes | easy | B |
| Instance | transform child mesh | easy | B |
| Grid | inherits via Composite | free | B |
| Scene | inherits via Composite | free | B |
| Sphere | UV sphere, `lod` → band count | medium | C |
| Torus | ring×ring grid, `lod` → both ring densities | medium | C |

Each batch worker gets:
- Phase 1 architecture (extended `Mesh.h`, `Primitive::tessellate` signature,
  `Box` reference impl + test).
- The CLAUDE.md "Adding a new visible-output feature" workflow, scoped to
  "this isn't a new visible feature, it's tessellate impls — focus on the
  tests and the math."
- Their assigned primitive list.
- One commit per batch.

## Phase 3 — wrap-up (sequential, after batches return)

- Merge any conflicting test or header additions across batches.
- Verify all primitives have tests; full test suite passes.
- Integration test: tessellate every primitive in a small scene, count
  total triangles, sanity-check.
- CHANGELOG entry (one line in `Added` covering the whole feature).
- Mark roadmap §3.R4 done.

## Out of scope (deferred per roadmap)

- **New primitives** the roadmap mentions for §3.R4 that don't exist yet
  (Cone, Capsule). Add them in a follow-up — not blocked by R4 itself.
- **CSG mesh booleans** (libigl / BSP). Separate epic per roadmap §4.2.a.
- **Marching cubes for SDF.** No SDF primitives exist yet.
- **The actual engines** that consume tessellated meshes (wireframe,
  software raster, OpenGL). Those are §4.1, gated on R5 (`RenderEngine`
  abstraction).
