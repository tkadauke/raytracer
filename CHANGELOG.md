# Changelog

All notable behavior-affecting changes to this project will be documented in
this file. The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Each entry attributes the change to its author (a person or an AI agent name).
This convention exists so agent-driven changes can be audited end-to-end —
see `docs/modernize.md` §3.11 and `CLAUDE.md` for the rules.

## Unreleased

### Added

- **Molecule representation and color import options.** Molecule imports now
  expose ball-and-stick, space-filling, and backbone representations plus
  element, chain, and residue-category color schemes through the shared importer
  option surface for rendercli and Modeler for Epic #236. — GPT-5
- **Protein backbone curves.** Molecule imports can now generate per-chain CA
  trace backbone curves with residue segment metadata, selectable as overlay,
  ribbon, tube, or disabled output through importer options for Epic #236. —
  GPT-5
- **BoundingBox ray tests can reuse precomputed inverse directions and return slab intervals.** `BoundingBox` now exposes precomputed-inverse overloads for boolean and interval ray tests, and scalar BVH traversal computes that inverse once per ray before walking node boxes. — GPT-5
- **Scene acceleration policy.** World scenes now expose an explicit
  acceleration mode for Auto, Linear, Grid, or BVH selection; converted runtime
  scenes record the selected acceleration decision for diagnostics while Auto
  conservatively preserves the existing Grid default for Epic #360. — GPT-5
- **`render::SpatialIndexFactory` acceleration selection.** Callers can now
  construct the linear `Composite` fallback, `Grid`, or `BVH` through one
  abstraction without depending on the concrete accelerator type for Epic #360.
  — GPT-5
- **`render::SpatialIndex` acceleration-structure interface.** `Composite`,
  `BVH`, and `Grid` now expose the shared add/setup/bounds/intersect contract
  explicitly without changing default scene construction for Epic #360. —
  GPT-5
- **Integrator contract.** Runtime ray rendering now has a narrow
  `render::Integrator` interface documenting single-ray radiance evaluation
  over a scene, ray, mutable render state, and recursive `RayCaster` callback
  for Epic #357. — GPT-5
- **Whitted integrator.** The recursive Whitted light-transport policy now
  lives in `render::WhittedIntegrator`, preserving `Raytracer::rayColor`
  results while giving future ray engines a concrete integrator boundary for
  Epic #357. — GPT-5
- **Raster counter AOVs.** The rasterizer and render graph now expose
  graph-visible heatmap views for per-pixel coverage, depth tests, depth passes,
  shading calls, and color writes through rendercli and the Modeler Preview
  View menu, using an absolute color scale so red marks high repeated raster
  work rather than the image-local maximum. — GPT-5
- **glTF mesh/material import fixtures and smoke tests.** glTF imports now
  compile supported triangle meshes into `MeshPrimitive` geometry, map
  base-color factors and `TEXCOORD_0` base-color textures to matte materials,
  render a representative glTF fixture through rendercli, and document the
  supported subset and unsupported extensions for Epic #233. — GPT-5
- **glTF animation import smoke path.** glTF imports now preserve animation
  sampler/channel metadata on node groups, convert simple translation,
  rotation, and scale channels into world timeline tracks where possible, and
  report diagnostics for unsupported interpolation or target paths for Epic
  #233. — GPT-5
- **glTF cameras and punctual lights.** glTF imports now parse camera records
  and `KHR_lights_punctual`, mapping perspective cameras to `PinholeCamera`,
  orthographic cameras to `OrthographicCamera`, directional/point lights to
  world lights, and warning when spot lights or range attenuation cannot be
  represented directly for Epic #233. — GPT-5
- **glTF product-view scene defaults.** Direct `.gltf` and `.glb` imports now
  use shared importer scene defaults for front-facing upright product-view
  orientation, white background, ambient fill, directional light, and
  unit-scale-aware pinhole camera framing in Modeler and rendercli. — GPT-5
- **Source asset animation.** Scene animation tracks can now target editable
  source/import parameters such as OpenSCAD Customizer values; numeric
  parameters interpolate, boolean/string-like parameters are step-only, and
  `SourceAsset` rebuilds generated output when animated values are applied. —
  GPT-5
- **Modeler Open Recent.** The File menu now includes an Open Recent submenu
  backed by the ten most recent scene/import files, so frequently used scenes
  and source assets can be reopened without returning to the file dialog. —
  GPT-5
- **Modeler Import action.** Modeler now has `File -> Import` for adding a
  supported external model, including `.scad`, to the current scene without
  replacing the scene or applying standalone-open camera, lighting, background,
  or render-setting defaults. — GPT-5
- **Source asset material overrides.** Source-backed imports such as direct
  OpenSCAD `.scad` assets now expose a material reference in Modeler and scene
  JSON, applying it as a whole-asset override to generated geometry while
  keeping importer-generated children transient. — GPT-5
- **Property editor search and collapsible groups.** The Modeler property editor
  now filters properties as you type, supports collapsible sections, and keeps
  property controls compact when there is extra vertical space. — GPT-5
- **Editable OpenSCAD source parameters.** Direct `.scad` opens in Modeler now
  stay source-backed, expose OpenSCAD Customizer sections, tooltips, numeric,
  boolean, string-choice, vector-expression assignments, human-readable labels,
  and numeric precision hints in the property inspector, and rebuild the
  generated mesh when those values change. — GPT-5
- **Modeler importer open filter.** The Modeler Open dialog now builds its
  default scene/import filter from registered importer extensions, so `.scad`
  files are selectable without switching to “All Files”. — GPT-5
- **OpenSCAD product-view scene defaults.** Direct OpenSCAD file imports now use
  shared importer scene defaults for white background, ambient fill, directional
  light, product-view upright orientation, and pinhole camera framing in Modeler
  and rendercli. — GPT-5
- **OpenSCAD workflow fixtures and docs.** Checked-in OpenSCAD fixtures now
  cover primitive, transform, and boolean sources for external-compiler and
  native-subset workflows; rendercli smoke tests reuse those fixtures and skip
  the real external compiler smoke when `openscad` is not installed. — GPT-5
- **glTF scene and node Group import.** Registered `.gltf` and `.glb` imports
  now map glTF scenes and nodes to editable `Group` hierarchy with node names,
  local transforms, optional hierarchy flattening, and source provenance
  metadata for Epic #233. — GPT-5
- **glTF mesh primitive import.** glTF node meshes now import triangle
  primitives as shared `MeshPrimitive` geometry with positions, indices,
  normals, UVs, computed-normal/zero-UV fallbacks, and material color
  references for Epic #233. — GPT-5
- **glTF material texture import.** glTF PBR base-color factors and
  base-color textures now map to renderer `MatteMaterial` diffuse textures,
  external texture images resolve through the glTF asset resolver, and
  unsupported metallic/roughness, alpha, double-sided, and extension features
  emit import warnings for Epic #233. — GPT-5
- **Cached LDraw filesystem resolution.** Repeated LDraw part/subfile lookups now
  reuse resolver results instead of rescanning the library roots for every
  cache-key/open call, making large MPD imports much faster. — GPT-5
- **LDraw product-view scene defaults.** Direct LDraw file imports now use the
  shared scene importer for front-camera framing, white background, stronger
  ambient fill, native-coordinate default orientation, summarized import
  warnings, and `rendercli --ldraw-background-color <color-name|hexvalue>`. —
  GPT-5
- **Modeler LDraw file import.** `.ldr`, `.dat`, and `.mpd` files now have a
  registered scene importer that creates a world scene shell, resolves LDraw
  geometry in the background, frames the active pinhole camera, and is available
  from the Modeler Open dialog and rendercli extension-based import path. —
  GPT-5
- **Native OpenSCAD CSG subset import.** `.scad` source assets now import
  `translate`, `rotate`, `scale`, `union`, `difference`, `intersection`,
  `cube`, `sphere`, and `cylinder` directly into editable primitives and CSG
  objects with source-located diagnostics for unsupported syntax for Epic #234.
  — GPT-5
- **OpenSCAD compiled mesh import.** OpenSCAD source asset imports now select
  the generated mesh reader by output format, support STL and PLY compiled
  output, attach source provenance to generated mesh nodes, and render through
  rendercli with deterministic default material for Epic #234. — GPT-5
- **OpenSCAD source asset import.** Registered `.scad` imports now use an
  optional external OpenSCAD executable to compile cached STL mesh output,
  report missing-tool diagnostics without throwing, and attach generated mesh
  primitives for Epic #234. — GPT-5
- **Source-backed asset scene objects.** Scene JSON can now persist
  `SourceAsset` objects with source path, importer format, import options,
  generated-output cache key, and non-fatal import diagnostics for Epic #234.
  — GPT-5
- **STL mesh import.** ASCII and binary STL files now import through the
  shared scene importer pipeline as flat `MeshPrimitive` geometry, with binary
  triangle-count validation and diagnostics for STL's unitless/no-material
  assumptions for Epic #235. — GPT-5
- **3MF core package import.** Registered `.3mf` imports now read core ZIP
  packages, parse model XML meshes, build-item transforms, units, and base
  material colors, and instantiate them as grouped imported mesh geometry for
  Epic #235. — GPT-5
- **Additive manufacturing fixtures and import smoke tests.** Tiny
  slicer-independent STL, 3MF, and G-code fixtures now exercise direct
  `rendercli` model import and G-code layer visualization, with docs covering
  the supported G-code subset and additive mesh non-goals for Epic #235. —
  GPT-5
- **G-code additive manufacturing visualization modes.** Imported G-code
  toolpaths can now color by layer, tool, speed, temperature, or
  extrusion/travel mode, and `rendercli` exposes G-code layer filtering and
  travel hiding controls for Epic #235. — GPT-5
- **G-code toolpath curve importer.** Parsed G-code now compiles into visible
  travel and extrusion curves grouped by layer, tool, and slicer feature
  metadata, with segment speed and extrusion attributes available for curve
  coloring and inspection for Epic #235. — GPT-5
- **G-code parser for printer path visualization.** `core/formats/gcode`
  now parses common 3D-printer movement, extrusion, feed-rate, layer comment,
  temperature, and tool-change commands while preserving diagnostics for
  unsupported dialect commands for Epic #235. — GPT-5
- **Low-level glTF asset reader.** `core::gltf::Reader` now parses `.gltf`
  JSON and `.glb` containers into typed buffers, bufferViews, accessors, and
  image references, resolves external and embedded payloads, and reports
  structured diagnostics for malformed assets for Epic #233. — GPT-5
- **Grouped molecule scene import.** PDB and PDBx/mmCIF imports now compile
  models, chains, and residues into nested generic `Group`s with molecule
  metadata and import provenance, while atom spheres remain hideable through
  chain or residue group visibility for Epic #236. — GPT-5
- **Ball-and-stick molecule scene builder.** Parsed molecules can now produce
  renderable model/chain/residue `Group` hierarchies with element-colored atom
  spheres and gray bond cylinders, using PDB `CONECT` records when present and
  covalent-radius distance inference otherwise for Epic #236. — GPT-5
- **Molecular coordinate parsing.** Core molecule parsing now reads PDB
  `ATOM` / `HETATM` and supported PDBx/mmCIF `_atom_site` coordinate records
  into atoms, residues, chains, models, metadata, and parse diagnostics for
  Epic #236. — GPT-5
- **LDraw validation fixtures and docs.** Tiny checked-in LDraw fixtures now
  cover inline geometry, library part resolution, nested subfiles, inherited
  color, BFC, and MPD import, with rendercli smoke coverage and textbook docs
  for library-path configuration and supported importer limits for #210. —
  GPT-5
- **Explicit LDraw import options.** World scene LDraw metadata and direct
  `rendercli --ldraw_input` now expose library root, import scale, coordinate
  conversion, hierarchy preservation, normal mode, edge-overlay inclusion,
  recursion limit, and missing-part policy for #210. — GPT-5
- **rendercli grouped step rendering.** `rendercli --step` now renders grouped
  scene step selections in single, cumulative, and sequence modes for Epic
  #232. — GPT-5
- **Modeler indexed preview scrubbing.** The Modeler now detects grouped scenes
  with generic step/layer/time metadata and enables a preview Index scrubber
  that filters group visibility without mutating saved explicit visibility. —
  GPT-5
- **Step playback highlight and ghost modes.** Group `stepIndex` metadata can
  now drive optional render-time active-step highlighting and previous-step
  ghosting through `rendercli --step`, `--step_highlight`, and
  `--step_ghost_previous`, while default rendering remains unchanged. — GPT-5
- **LDraw edge-line overlays.** `LDrawGeometryCompiler` now carries type-2
  edge/detail lines as zero-width curve-overlay segments with color 24
  resolved from the active part edge color, while type-5 optional lines remain
  explicitly ignored with diagnostics for #210. — GPT-5
- **LDraw source provenance metadata.** Runtime objects can now carry opaque
  inspection metadata, and imported LDraw meshes/type-1 instances record source
  file or MPD block, line range, command type, color code, build step,
  referenced part, and parent reference details without changing render output
  for #210. — GPT-5
- **LDraw TEXMAP texture import.** The LDraw parser now preserves `!TEXMAP`
  START/NEXT/FALLBACK/END metadata, and the geometry compiler can render
  simple planar texture maps with image-backed UV materials while reporting
  missing textures and using fallback geometry for unsupported projections for
  #210. — GPT-5
- **Structured LDraw import diagnostics.** LDraw resolving and geometry
  compilation can now collect machine-checkable warnings/errors for missing
  subfiles, unsupported commands, skipped line geometry, color fallbacks, BFC
  sidedness/winding treatment, and fatal parse failures for #210. — GPT-5
- **LDraw hierarchy-preserving imports.** LDraw imports can now preserve
  `0 STEP` sections and MPD submodel references as generic scene `Group`
  nodes with source/build metadata, while the flattened metadata import path
  remains available for #210. — GPT-5
- **World and rendercli LDraw imports.** Scene JSON can now contain
  `Collection` authoring metadata for LDraw source files; rendercli resolves
  that metadata through the import pipeline into ordinary compiled primitive
  geometry, and direct LDraw input remains available with
  `--ldraw_library_root` for #210. — GPT-5
- **Shared import provenance metadata.** World scene elements now carry
  optional structured metadata, and import helpers can attach source file,
  source entity, line range/record, original-unit, and category provenance that
  round-trips through scene JSON for Epic #230. — GPT-5
- **Importer-powered scene loading.** `rendercli` can now load registered
  importers by `--import_format` or filename extension, and world scene JSON
  can expand top-level `imports` entries with source paths and importer
  options for Epic #230. — GPT-5
- **Shared import asset resolver.** `core::AssetResolver` now resolves
  importer sidecar files from the current file directory and configured search
  roots, returns cache-stable asset identities, and reports requested paths plus
  searched roots in missing-file diagnostics for Epic #230. — GPT-5
- **Shared scene importer interface.** `world::SceneImporter`,
  `ImportOptions`, `ImportResult`, and `ImportDiagnostic` now define a
  format-neutral import contract with option schemas, source metadata,
  diagnostics, and owned scene/group roots for Epic #230. — GPT-5
- **Group step visibility evaluator.** `StepVisibilityEvaluator` now evaluates
  group visibility for only-step, cumulative, all-steps, and step-range modes
  while composing explicit group visibility and nested ancestors. — GPT-5
- **Group step/time metadata helpers.** `Group` now exposes generic
  `stepIndex`, `layerIndex`, `startTime`, `endTime`, and `label` metadata
  helpers for importer-agnostic ordered steps, layers, frames, and time
  intervals. — GPT-5
- **Shared imported mesh assets.** `core::MeshAsset` and
  `render::MeshPrimitive` now give importers a shared mesh ownership boundary
  with per-face runtime material assignment that works through raytracing,
  Grid/BVH traversal, rasterization, and wireframe previews for Epic #230. —
  GPT-5
- **Importer fixture harness and lifecycle docs.** Shared importer test helpers
  now assert diagnostics and imported group trees, fixture layout supports
  sidecar assets beside importer sources, and the textbook documents importer
  options, asset resolution, diagnostics, provenance, and render smoke patterns
  for Epic #230. — GPT-5
- **Group/Collection documentation and fixtures.** Group source docs and the
  scene-structure textbook now describe hierarchy-only collections, visibility,
  metadata, and the distinction from render layers/AOVs; a reusable nested
  group fixture covers importer-facing transform and visibility behavior. —
  GPT-5
- **LDraw compiled part reuse.** `LDrawGeometryCompiler` now exposes cache
  counters and reuses compiled subfile geometry across repeated type-1
  references with matching resolved file, color context, and winding state,
  while instances preserve per-reference transforms, bounds, and materials for
  #210. — GPT-5
- **Modeler render settings editor.** The Modeler Elements tree now exposes the
  scene's saved render settings as a generated item, preview controls can
  either use that saved intent or layer temporary overrides, and the Render
  window shows the final compiled graph before executing that same graph. The
  property editor now displays grouped, human-readable render settings with
  dropdowns for enumerated choices and backend-specific fields filtered by the
  selected engine, and uses compact vertical value editors so the dock can be
  narrower and scrolls long property sets instead of expanding eagerly. Modeler
  now hides low-level view-plane/thread/queue controls while constraining
  numeric render settings to valid editor ranges. — GPT-5
- **Render graph engine options.** `RenderIntent` now carries typed raytracer,
  rasterizer, and wireframe engine options, rendercli and the Modeler final
  render window compile those options into graph pass state, and subview
  intents can inherit or override global engine options. — GPT-5
- **rendercli graph view overrides.** `rendercli --render_graph_view_override`
  can now append high-level `RenderViewOverride` intent records from the
  command line, with whole-frame overrides applied by the current compiler and
  selector-specific overrides rejected clearly until scene partitioning is
  implemented. — GPT-5
- **rendercli graph depth/id external inputs.** Explicit graph replay can now
  bind imported or history depth, object-id, and material-id resources from
  image files with `--render_graph_depth_in`, `--render_graph_object_id_in`,
  and `--render_graph_material_id_in`, completing CLI coverage for the CPU
  external-input types already supported by `GraphRenderEngine`. — GPT-5
- **Render graph scene analysis.** `world::Scene` now feeds
  `RenderSceneAnalysis` into shared graph compilation, giving the compiler
  scene-derived facts such as visible geometry and lights before it synthesizes
  feature nodes; raster preview shadow nodes are skipped when the analyzed scene
  cannot use them. — GPT-5
- **Shared render graph request resolution.** rendercli and Modeler preview now
  layer scene intent and temporary graph overrides through
  `RenderGraphRequest`, and the raster render dialog uses the same compilation
  entry point, reducing CLI/UI divergence and letting Modeler preview controls
  explicitly clear scene-requested AA, shadows, and overlays. — GPT-5
- **Render graph stencil-composite view mode.** Render intent and
  `rendercli --render_graph_view stencil_composite` now synthesize raster
  beauty, wireframe foreground, raster stencil AOV, stencil composite, tonemap,
  and exported stencil-preview nodes; Modeler can select the view from the
  Preview menu and ships with `scenes/render_graph_stencil_composite_demo.json`.
  — GPT-5
- **Render graph Modeler demo scene.** `scenes/render_graph_aov_demo.json`
  opens directly in Modeler with a saved rasterizer graph intent, SMAA
  postprocess node, and stencil AOV side branch for graph-inspection demos. —
  GPT-5
- **rendercli graph external image inputs.** `rendercli --render_graph_color_in
  resource=file` and `--render_graph_stencil_in resource=file` now bind imported
  or history graph color and stencil resources from image files before replaying
  explicit graph JSON. — GPT-5
- **Graph stencil AOV view.** Render intent can now request a `stencil` AOV:
  rendercli, Modeler preview, graph JSON, execution traces, and side-output AOV
  exports synthesize and visualize graph-visible stencil masks, with rasterizer
  plans using tessellated raster geometry through a stencil-marking payload. —
  GPT-5
- **Depth/stencil graph composite pass.** Built-in `Composite` executor passes
  tagged with `depth_composite` or `stencil_composite` now combine base and
  foreground color resources through graph-visible depth and stencil resources,
  and `GraphRenderEngine` can bind external stencil inputs for imported/history
  graph resources. — GPT-5
- **Curve/path documentation and fixtures.** Public curve docs and the
  tessellation textbook now describe polyline data, ribbon/tube tessellation,
  curve overlays, and attribute-color rendering; reusable plain and attributed
  curve fixtures plus render smoke tests cover downstream importer paths. —
  GPT-5
- **Curve overlay rendering mode.** `render::Curve` can now emit semantic
  center-line overlay segments independently of physical tessellation, and
  `rendercli --render_graph_curve_overlay` draws them as thin graph-visible
  debug strokes over raytraced, raster, or wireframe beauty output. — GPT-5
- **Curve segment attribute color mapping.** `core::AttributeColorMap` maps
  scalar and categorical curve segment attributes to deterministic colors, and
  `render::Curve` writes those colors as mesh face overrides while missing
  attributes continue to use the curve's default material/color. — GPT-5
- **Runtime curve primitive tessellation.** `render::Curve` now wraps
  `core::Polyline` paths and tessellates finite-width curves into ribbon or
  tube meshes for raster, wireframe, and other mesh-consuming render paths,
  while safely skipping zero-length segments. — GPT-5
- **Render graph external color inputs.** `GraphRenderEngine` can now bind
  imported or history CPU color resources for explicit plans, giving replayed
  temporal/postprocess graphs a real external-input path. — GPT-5
- **Render graph external depth inputs.** `GraphRenderEngine` can now bind
  imported or history CPU depth resources for explicit plans, so depth
  visualization and future depth-history graph paths can use supplied inputs.
  — GPT-5
- **Render graph external integer-id inputs.** `GraphRenderEngine` can now bind
  imported or history CPU object-id/material-id resources for explicit plans.
  — GPT-5
- **LDraw MPD submodels.** `LDrawParser` now splits `0 FILE` / `0 NOFILE`
  MPD blocks into named virtual files, and `LDrawGeometryCompiler` resolves
  MPD-local type-1 submodel references before external LDraw library roots for
  #210. — GPT-5
- **LDraw BFC winding import.** `LDrawGeometryCompiler` now honors `0 BFC`
  `CERTIFY`, `NOCERTIFY`, `CW`, `CCW`, `CLIP`, `NOCLIP`, and `INVERTNEXT`
  meta commands so certified clipped polygons produce front-sided materials,
  uncertified or no-clip polygons remain two-sided, and inverted subfile
  references reverse imported face winding and normals for #210. — GPT-5
- **Recursive LDraw subfile references.** `LDrawGeometryCompiler` now resolves
  type-1 subfiles through an `LDrawFileResolver`, applies LDraw affine
  transforms through runtime instances, preserves color-16 inheritance with
  direct child color overrides, and guards recursive imports with cache-backed
  cycle and depth checks for #210. — GPT-5
- **Inline LDraw mesh compilation.** `LDrawGeometryCompiler` now converts
  parsed type 3 triangles and type 4 quads into renderable `MeshPrimitive`
  geometry with LDraw color-table material assignment for #210. — GPT-5
- **Modeler group editing.** Modeler can now create `Group` nodes, show their
  transform, visibility, name, and metadata in the property editor, and reparent
  scene-tree children into and out of groups while preserving global transforms.
  — GPT-5
- **Group metadata JSON.** `world::Group` now carries optional structured
  importer metadata that round-trips through scene JSON without affecting
  rendering. — GPT-5
- **World scene groups.** `Group` scene objects can organize surfaces, lights,
  and nested groups with transform and visibility controls, converting visible
  geometry to runtime composites without abusing inactive CSG surfaces. — GPT-5
- **Owned runtime mesh primitive.** `render::MeshPrimitive` now owns or shares
  imported `Mesh` geometry while building flat or smooth triangle leaves for
  intersection, tessellation, and accelerator traversal. — GPT-5
- **Core polyline geometry model.** `core::Polyline` now stores ordered 3D
  points, per-curve typed metadata, per-segment typed metadata, segment
  iteration, and bounding-box computation for path-like geometry. — GPT-5
- **Render graph trace cache flags.** Execution trace JSON cache metadata now
  includes `cacheable`, `usedCachedArtifact`, and `storedCachedArtifact`
  booleans alongside the status string. — GPT-5
- **Render graph intent helpers.** `RenderIntent` and `ShadingProfileRef` now own
  common mutations for exported AOV requests and shading-profile parameters, so
  tools no longer manipulate those containers directly. — GPT-5
- **rendercli graph shading-profile override.**
  `rendercli --render_graph_shading_profile <name>` now overrides the default
  graph intent shading profile before compilation, and repeated
  `--render_graph_shading_parameter key=value` options attach parsed scalar
  parameters so exported scene-rendering passes show the requested profile in
  their `SceneView`. — GPT-5
- **rendercli graph camera override.** `rendercli --render_graph_camera <id>`
  now overrides the render intent's default scene-camera reference in compiled
  graph plans. — GPT-5
- **LDraw color/material lookup.** `core/formats/ldraw` now parses
  `LDConfig.ldr` `0 !COLOUR` records, resolves LDraw current/edge color
  inheritance and direct RGB color codes, and maps standard finishes to
  renderer material approximations for #210. — GPT-5
- **RenderPlan pass dependency queries.** `RenderPlan` now exposes incoming and
  outgoing dependency-edge lookups for a selected pass. — GPT-5
- **Core LDraw parser.** `core/formats/ldraw` now parses `.dat` and `.ldr`
  text lines into typed records for LDraw line types 0 through 5, preserving
  type-1 trailing filenames and reporting malformed numeric fields with
  line-numbered `LDrawParseError`s. — GPT-5
- **LDraw subfile resolver.** `core/formats/ldraw` can now resolve and cache
  referenced `.dat`/`.ldr` subfiles from the current model directory and an
  external LDraw library root, including case-insensitive lookup, recursive
  preload, cycle detection, and actionable missing-file errors. — GPT-5
- **Render graph execution stages.** `RenderPlan` now exposes dependency-ready
  execution stages, text/JSON/DOT exports surface those stages, and the Modeler
  graph view uses them for its left-to-right layout of independent branches.
  — GPT-5
- **Modeler preview AOV views.** The Modeler Preview menu can now switch the
  live graph preview between beauty, depth, normal, object-id, material-id, and
  world-position views so the Render Graph dock shows the corresponding AOV
  nodes during interactive preview. — GPT-5
- **Graph multi-AOV output files.** Render intents can request exported AOV side
  branches and rendercli can write repeated `--render_graph_aov_out view=file`
  preview images without enabling traces for ordinary graph renders. — GPT-5
- **Graph world-position AOV view.** `--render_graph_view world_position` now
  compiles a graph-visible `world_position_aov` resource containing raw world
  coordinates plus a normalized visualization pass for display. — GPT-5
- **Graph material-id AOV view.** `--render_graph_view material_id` now compiles
  a graph-visible `material_id_aov` resource and visualization pass, using the
  same trace-inspectable integer-id path as object-id AOVs. — GPT-5
- **Graph object-id AOV view.** `--render_graph_view object_id` now compiles a
  graph-visible `object_id_aov` resource and visualization pass, and execution
  traces colorize object-id resources for inspection. — GPT-5
- **Graph normal AOV view.** `--render_graph_view normal` now compiles a
  graph-visible `normal_aov` resource and visualization pass, so normals can be
  rendered and inspected through the same graph path as depth AOVs. — GPT-5
- **Graph depth AOV view.** `--render_graph_view depth` now compiles a
  graph-visible `depth_aov` resource and `visualize_depth_aov` pass, letting
  rendercli and graph inspection tools render and inspect primary depth through
  the graph. — GPT-5
- **Modeler render graph grouped toggles.** The Render Graph dock now has a
  Groups tab for disabling all passes of a present kind, executor, or feature
  through the same effective-plan override path as individual pass toggles.
  — GPT-5
- **Modeler render graph export.** The Render Graph dock can now export the
  effective graph as text, DOT, or JSON through the Modeler save-file flow.
  — GPT-5
- **Raster shadow-map artifact cache.** Graph preview shadow passes now cache
  the full directional shadow-map collection, restore it when pass state,
  descriptor, camera, scene, and light inputs still match, and feed the cached
  artifact into raster beauty instead of rebuilding shadows internally. —
  GPT-5
- **Graph-visible raster shadow depth traces.** Raster preview shadow passes now
  materialize the first directional-light cascade as a CPU depth resource, and
  the Modeler Graph Trace preview renders depth snapshots as grayscale images
  instead of treating shadow maps as metadata-only resources. — GPT-5
- **Render graph trace cache metadata.** Execution trace resource snapshots now
  report cache status in JSON and in the Modeler resource property view, so
  graph tooling can distinguish ordinary non-cacheable resources from
  persistent-cache resources that were not served by an artifact. — GPT-5
- **Render graph artifact cache.** `GraphRenderEngine` now owns a clone-shared
  `RenderGraphArtifactCache` with typed cache keys for immutable persistent
  artifacts, establishing the cache surface needed for future shadow-map and
  probe reuse. — GPT-5
- **Render graph trace input freshness.** Execution traces now carry a
  render-input fingerprint, and the Modeler only shows a completed trace when
  both the graph plan and current preview inputs still match, avoiding stale
  per-node images after camera, scene, background, or tonemap changes. — GPT-5
- **Modeler graph-node property inspection.** Selecting a render graph pass or
  resource node now makes that node the active read-only Properties target, with
  pass settings, resource descriptors, and available trace metadata surfaced
  from the graph view; the redundant Dependencies tab was removed because the
  graph already shows those edges. — GPT-5
- **Modeler graph trace preview.** Selecting a render graph pass or inspectable
  color resource now opens a large central Graph Trace preview with input,
  output, and difference tabs for the last graph execution. — GPT-5
- **Render graph execution traces.** `GraphRenderEngine` can now record the
  last executed plan's pass status, timing, CPU color input/output previews, and
  simple color difference previews so inspection tools can explain what each
  graph node read and wrote. — GPT-5
- **Rendercli graph trace export.** `rendercli --render_graph_trace_out` writes
  the last executed graph trace as JSON alongside a graph-backed image render.
  — GPT-5
- **Render graph trace session safety.** Execution traces now ignore events from
  retired render workers after a newer graph render has started, preventing
  stale preview jobs from overwriting the latest trace. — GPT-5
- **Render graph live-event generations.** Live graph execution observer events
  now carry a render generation so the Modeler preview ignores pass updates from
  retired workers once a newer render has been requested. — GPT-5
- **Rendercli image probe assertions.** Rendercli CMake tests now use a
  Qt-backed image probe to assert decoded dimensions, nonzero RGB pixels, and
  stable raw-pixel hashes without comparing PNG bytes. — GPT-5
- **Render graph text dependencies.** Text graph exports now include declared
  pass-to-pass dependencies and the dependency-derived execution order
  separately from pass declaration order, so replayed or hand-authored plans can
  show how the serial graph executor will run them. — GPT-5
- **Render graph DOT disabled styling.** DOT graph exports now draw disabled
  pass nodes with dashed gray styling so graph diagrams show effective
  overrides without needing a separate text dump. — GPT-5
- **Render graph text features.** `RenderPlan::toText()` now lists each pass's
  feature tags, making rendercli graph dumps show the exact tags affected by
  `--disable_feature`. — GPT-5
- **Graph-visible raster preview shadow pass.** Raster graph plans with preview
  shadows now include a `raster_preview_shadows` shadow node and
  `preview_shadow_map` resource before `raster_beauty`; disabling that node
  substitutes the default and prevents graph-controlled shadow enablement in
  the beauty payload. — GPT-5
- **Graph resource substitution provenance.** Runtime graph resources now record
  whether their current contents came from disabled-pass substitute-default
  behavior or from a normal producer/passthrough, giving later passes a typed
  way to distinguish real graph inputs from defaults. — GPT-5
- **Graph-visible wireframe pass state.** Wireframe beauty and overlay graph
  passes now carry typed pass state, and graph-backed rendercli replays
  `--lod` through that state instead of only applying it in direct wireframe
  engine mode. — GPT-5
- **RenderPlan graph queries.** `RenderPlan` now exposes typed lookup helpers
  for pass ids, resource ids, resource producers, resource consumers, and
  declared pass-to-pass dependencies, giving later graph-rewrite code and
  inspection tools a single plan-owned API for dependency queries. — GPT-5
- **RenderPlan edge construction.** `RenderPlan` now exposes plan-owned helpers
  for adding resource producers, connecting producer passes to consumers, and
  routing resources through inserted passes, so compiler code can build graph
  edges without open-coded pass-list surgery. — GPT-5
- **Typed graph postprocess AA state.** Graph-visible FXAA/SMAA passes
  now serialize `post_process_aa` pass parameters at the JSON boundary and
  execute from typed C++ state rather than inferring the filter from a pass id.
  — GPT-5
- **Graph-visible image AA passes.** Graph-backed renders now compile
  `--post_aa fxaa` and `--post_aa smaa` into explicit `post_fxaa` /
  `post_smaa` postprocess passes between beauty and overlay/tonemap for
  raytracer, wireframe, and rasterizer executors, while `--post_aa taa` remains
  on raster beauty until temporal history resources are graph-owned. — GPT-5
- **Typed graph raster shadow pass state.** The graph-visible
  `raster_preview_shadows` node now serializes and replays raster shadow-map
  settings on the shadow pass itself, and graph execution passes that typed
  request through the runtime shadow resource to `raster_beauty`. — GPT-5
- **Graph-visible raster pass state.** Raster-backed graph renders now compile
  rendercli raster controls into typed `raster_beauty` pass state and serialize
  that state at the JSON boundary, covering MSAA, MSAA shading, post-process
  AA, culling, queue/thread controls, viewport/scissor, fixed-function color
  output, alpha test, depth bias, and direct raster shadow-map settings. — GPT-5
- **Commit-time clang-format hook.** The repository now includes a tracked
  `.githooks/pre-commit` hook that formats staged C/C++ files before each
  commit, and the pre-commit clang-format configuration now runs in normal fix
  mode against the formatted baseline. — GPT-5
- **Scene render-intent JSON.** `world::Scene` now reads and writes an optional
  top-level `renderIntent` block backed by `RenderIntent::toJson()` /
  `RenderIntent::fromJson(...)`; rendercli and Modeler use that saved intent as
  the base for graph compilation before applying preview or command-line
  overrides. — GPT-5
- **Render graph wireframe overlay pass.** `RenderIntent::enableWireframeOverlay`,
  `rendercli --render_graph_wireframe_overlay`, and the Modeler preview menu now
  compile a graph-visible `wireframe_overlay` pass between beauty and tonemap,
  with disabled-pass passthrough support for node-toggling education. — GPT-5
- **Graph-backed Modeler preview.** The central `Modeler` preview now renders through `GraphRenderEngine` using the Render Graph dock's effective plan when that plan validates. Pass checkboxes immediately affect valid preview graphs, `Render → Preview Tonemap` selects the graph tonemap node's operator, and graph LDR output packs after the graph so tonemap is not applied twice. — GPT-5
- **Render graph default tonemap pass.** `RenderGraphCompiler` now emits a two-pass default graph: the selected whole-frame beauty executor writes a transient `beauty_color` resource, then a `tonemap` postprocess pass writes the exported `main_color` resource. Disabling the tonemap pass uses passthrough validation/execution, giving the Modeler inspector and rendercli a real multi-node graph by default. — GPT-5
- **Modeler render-graph inspector.** `Modeler` now has a Render Graph dock
  that compiles the current live-preview graph, renders a left-to-right graph
  view with double-click pass toggles, lists pass/resource/dependency details
  including resource producers and consumers, and validates per-pass checkbox
  overrides before preview renders are started. — GPT-5
- **Modeler render-graph execution highlights.** `GraphRenderEngine` now emits
  live pass start/finish/failure events, and the Modeler Render Graph dock uses
  them to highlight graph nodes while preview renders are executing. — GPT-5
- **Render graph intent overrides.** `rendercli --render_graph_executor` and `--render_graph_view` now override the default graph intent executor/view mode before compilation, making graph inspection independent of the direct `--engine` shortcut. — GPT-5
- **Render graph JSON replay.** `RenderPlan::fromJson(...)` can rebuild plans from the JSON emitted by `toJson()`, and `rendercli --render_graph_in plan.json` can validate, re-export, or render through a saved graph with the usual disable filters applied after loading; graph replay infers the output image size from the exported color resource unless matching `--width` / `--height` overrides are supplied. — GPT-5
- **Rendercli render-graph inspection.** `rendercli` can now compile/export graph plans in text, DOT, or JSON form, render through `GraphRenderEngine`, and apply graph disable filters by pass id, pass kind, executor, or feature before validation. — GPT-5
- **Graph disabled-pass execution.** `GraphRenderEngine` now executes simple color-resource plans with enabled tonemap passes plus disabled-pass `SubstituteDefault` and color `Passthrough` behavior, backed by a runtime `RenderResource` hierarchy for resource capabilities. — GPT-5
- **Graph-backed beauty rendering.** `RenderGraphCompiler` now emits the first executable whole-frame beauty plan and `GraphRenderEngine` can compile or accept that plan, validate it, execute exactly one enabled Raytracer/Rasterizer/Wireframe beauty pass, and expose the last plan for inspection. — GPT-5
- **Raster recursive-material fallback diagnostics.** Reflective materials now advertise a raster fallback of local Phong only, transparent materials advertise local Phong plus transmission-derived source alpha, and `rendercli --engine raster` warns when recursive reflection/refraction has been dropped from the preview. — GPT-5
- **Rasterizer scene-aware tiling default.** Rasterizer queue sizing now defaults to an automatic policy that uses projected triangle count, projected bounds, tile-list duplication, framebuffer size, worker count, and MSAA samples to choose tiled rendering only for measured win cases; explicit `setQueueSize(...)` and `rendercli --queue_size` still force the caller's choice. Closes rasterizer-v2 §7 / Epic #167. — GPT-5
- **Render graph foundation types.** The new `engine::graph` module defines render intent, scene selectors, graph resources, pass declarations, virtual pass payloads, plan validation, graph override disabling, text/DOT/JSON plan export, and a new textbook render-graph volume as the first CPU-only infrastructure slice for the future hybrid render graph. — GPT-5
- **Raster tangent-space normal mapping.** Matte/Phong materials now carry an optional normal texture for raster previews; the software rasterizer derives tangent/bitangent frames from triangle UVs, applies tangent-space normal maps during direct lighting, and falls back to geometric normals when UV tangents are unavailable. Closes roadmap §4.3.b/§4.3.c normal-map preview work. — GPT-5
- **Rasterizer temporal anti-aliasing.** `Rasterizer::PostProcessAA::TAA` now jitters repeated raster frames and accumulates them through managed color/depth history with reset handling for first frame, resize, and explicit invalidation; `rendercli --post_aa taa`, the Modeler render dialog, and rasterizer docs expose the new mode (advances roadmap §4.1.b). — GPT-5
- **Image texture filtering and mipmaps.** `render::ImageTexture` and its world/JSON wrapper now support explicit nearest, bilinear, and mipmapped sampling with clamp/repeat wrapping; the raster material path supplies UV gradients for mip selection and rendered docs show the filtering differences. — GPT-5
- **Rasterizer alpha test and source-alpha blending.** Built-in raster material shading now carries transient alpha from `TransparentMaterial` opacity and texture intensity into alpha testing and `SourceAlpha` / `OneMinusSourceAlpha` blend factors, so failed alpha tests skip color/depth writes and blended passes can use fragment-sourced opacity instead of only pass constants. — GPT-5
- **Rasterizer SMAA post-process anti-aliasing.** `render::postprocess::applySmaa` adds a first CPU SMAA-style luminance-edge blend for raster previews, exposed through `Rasterizer::PostProcessAA::SMAA`, `rendercli --engine raster --post_aa smaa`, the Modeler render dialog, rendered docs, and the rasterization textbook (closes roadmap §4.1.b / Epic #167). — GPT-5
- **Rasterizer per-fragment MSAA shading mode.** `Rasterizer::MSAAShadingMode::PerFragment`, `rendercli --msaa_shading per_fragment`, and the Modeler render dialog now offer a cheaper MSAA mode that keeps coverage/depth/stencil per sample while reusing the first passing shaded color per prepared triangle and pixel. `PerSample` remains the default. — GPT-5
- **3D rasterizer frustum-clipping widget.** Rasterizer API docs and the clipping textbook chapter now include an interactive frustum inspection widget that shows source geometry, generated clip vertices, and clipped output from an adjustable inspection view. — GPT-5
- **Screen-space raster clipping helpers.** `core::clipTriangleToRect(...)` and `core::fanTriangulateRasterClipPolygon(...)` now provide a 2D Sutherland-Hodgman viewport-clipping reference path for already-projected triangles, matching the rasterization textbook's teaching counterpart to homogeneous clipping. — GPT-5
- **Rasterizer attachment load/store state.** Direct `Rasterizer::render(Buffer<Colord>&)` calls now expose explicit color load/store operations plus borrowed depth/stencil attachments with clear/load/store/discard behavior, giving multi-pass raster effects a deterministic way to preserve or exchange pass resources. — GPT-5
- **Rasterizer pass depth bias.** `Rasterizer` and `rendercli --depth_bias` now expose a signed constant depth offset applied before camera-pass depth test/write, allowing multi-pass raster overlays to pull fragments forward or push them behind existing depth without changing shader-visible geometric depth. — GPT-5
- **Rasterizer viewport and scissor state.** `Rasterizer` and `rendercli` now expose framebuffer viewport and scissor rectangles so raster previews can project into subregions or discard fragments outside a fixed rectangle before depth/stencil and color output. — GPT-5
- **Rasterizer color-output state.** `Rasterizer` and `rendercli` now expose RGB color write masks and fixed-function blend state with source/destination factors, blend ops, and pass-constant blend color/alpha for multi-pass raster effects. — GPT-5
- **Rasterizer slope-scaled shadow bias.** Directional shadow-map comparisons now support an opt-in `shadowSlopeBias` term that adds receiver-angle-dependent depth tolerance on top of the existing constant bias, with controls in `rendercli --shadow_slope_bias`, the Modeler render dialog, API docs, rendered-doc sweeps, and the shadow-map textbook section. — GPT-5
- **Rasterizer diagnostic output buffers.** Direct `Rasterizer::render(Buffer<Colord>&)` calls can now attach borrowed depth, normal, primitive, material, face-id, and stencil buffers for pass diagnostics, picking experiments, and debug views. — GPT-5
- **Rasterizer near/far clip-depth controls.** `Rasterizer` now exposes configurable eye-relative near and far clip depths; the near plane defaults to 0.1, the far plane defaults to infinity, and finite far depths clip geometry before perspective divide just like the near and viewport planes. — GPT-5
- **Additional animated scene fixtures.** Five reusable scene JSON files now cover camera panning, directional-light sweeps, material/background color fades, motion-blur velocity changes, and step-interpolated visibility toggles. The rendercli animation smoke test renders their first and last frames to keep the fixtures loadable. — GPT-5
- **Modeler timeline preview controls.** Scenes with a top-level `animation` block now enable a read-only Timeline dock with a frame slider and spinbox. The central preview and render dialog evaluate a copied scene at the selected frame, while the property editor continues editing the base authoring scene. — GPT-5
- **`rendercli --animation` image sequences.** `rendercli` can now render a scene timeline to numbered image files using a printf-style output pattern, with optional `--frame_start`, `--frame_end`, and `--fps` overrides. The CLI reports per-frame progress and rejects missing frame placeholders, invalid ranges, static scenes, and ambiguous `--animation` combinations. — GPT-5
- **`rendercli --frame` animation evaluation.** `rendercli` can now render a specific evaluated animation frame before converting the world scene to runtime render objects. Static scenes still render with `--frame`, invalid frame arguments fail during CLI parsing, and an `animation_frame_demo.json` scene plus CTest smoke cover frame-specific output. — GPT-5
- **World scene animation timeline loading and evaluation.** Scene JSON can now carry a top-level `animation` block with fps, frame range, and id-targeted `Q_PROPERTY` tracks. `world::Timeline` and `world::AnimationTrack` preserve animation on scene save/load and can evaluate editable scenes at a frame for direct `double`, `Vector3d`, `Colord`, and step-only `bool` properties. — GPT-5
- **Core timeline and interpolation primitives.** New Qt-free timeline/keyframe types under `core::animation` and reusable interpolation policies under `core::math::interpolation` provide validated frame timelines, typed tracks, `step`/`linear`/`smoothstep` sampling, scalar/vector/color tests, interpolation-mode parsing, API widgets, and a new textbook animation chapter for the upcoming world and `rendercli` animation work. — GPT-5
- **Rasterizer shadow-cascade split widget.** The Rasterizer API docs and shadow-map textbook section now include an interactive cascade diagnostic that shows camera-depth split bands, per-cascade light-space map coverage, and raw-vs-texel-snapped cascade centers while panning the camera. — GPT-5
- **Modeler rasterizer preview shadows.** The live preview's Rasterizer engine now has a `Render → Preview Engine → Rasterizer Preview Shadows` toggle that enables directional shadow maps with four stabilized cascades for interactive shadow-map inspection. The render dialog keeps its separate explicit shadow settings. — GPT-5
- **Rasterizer cascaded shadow maps.** Directional-light shadow maps can now split scene bounds into 1-4 camera-depth cascades, building a tighter shadow map for each slice. The setting is exposed through `Rasterizer::setShadowCascadeCount`, `rendercli --shadow_cascades`, the Modeler render settings widget, unit and functional tests, API docs, rendered-doc sweeps, and the shadow-map textbook section. — GPT-5
- **Rasterizer PCSS shadow filtering.** Directional-light shadow maps now support an opt-in `Rasterizer::ShadowFilterMode::PCSS` mode that searches the configured shadow-filter kernel for blockers, estimates a receiver-local penumbra radius from blocker depth, and then runs PCF with that clamped radius. Existing PCF behavior remains the default. The mode is exposed through `rendercli --shadow_filter pcf|pcss`, the Modeler render settings widget, functional tests, API docs, rendered-doc sweeps, and the shadow-map textbook section. — GPT-5
- **Raster material preview scene.** `scenes/raster_material_preview.json` and the rasterizer rendered-doc image now provide a reusable visual check for Matte ambient/diffuse coefficients and broad-vs-tight Phong highlights in the software rasterizer. — GPT-5

### Changed

- **Direct LDraw camera framing.** Direct `rendercli --ldraw_input` scenes now
  frame the active pinhole camera from imported model bounds through shared
  `Scene`/`PinholeCamera` logic, giving large or offset models a full-model
  three-quarter view instead of a rendercli-only extent heuristic. — GPT-5
- **Render type-specific graph and raster hooks.** Runtime cameras, lights,
  tonemaps, and recursive materials now expose stable graph-cache and
  raster-preview capabilities through virtual methods instead of external RTTI
  checks in graph execution, shadow-map setup, and rendercli diagnostics. —
  GPT-5
- **Modeler render graph display names.** The Render Graph dock now shows
  human-readable pass and resource names in graph nodes and tables while
  keeping stable ids in item metadata, tooltips, and exported plans. — GPT-5
- **Modeler render graph scene-view node labels.** Pass nodes in the Render
  Graph dock now show non-default scene selector, camera, and shading-profile
  intent directly in the graph view, with long labels elided inside fixed node
  bounds. — GPT-5
- **Render graph shading-profile parameters.** `ShadingProfileRef` now stores
  parsed scalar parameter values instead of retaining raw `QJsonObject`
  parameters past the JSON import boundary. — GPT-5
- **Render graph shading-profile scene views.** Synthesized scene-rendering
  passes now carry non-default shading profile intent in their `SceneView`, and
  text, DOT, JSON, rendercli functional checks, and Modeler pass details expose
  that intent. — GPT-5
- **Modeler render graph pass hover details.** Pass-node tooltips in the
  Modeler Render Graph view now include scene selector and camera intent
  alongside dependency summaries. — GPT-5
- **Render graph DOT scene-view details.** DOT render-plan exports now include
  pass selector and camera labels when a pass has non-default scene-view intent.
  — GPT-5
- **Render graph active camera intent.** `world::Scene` now exposes the active
  camera as a render-graph camera reference, and rendercli/Modeler use it when
  scene intent does not name a default camera so compiled passes identify the
  scene camera they render through. — GPT-5
- **Render graph text scene-view details.** Text render-plan exports now show
  each pass's scene selector and camera reference, using the same graph-type
  display formatting as the Modeler inspector. — GPT-5
- **Modeler render graph scene views.** The Render Graph dock now shows each
  pass's scene selector and camera reference in the Passes table and selected
  pass property details. — GPT-5
- **Render graph pass camera intent.** Synthesized scene-rendering passes now
  carry the effective default camera reference in their `SceneView`, and plan
  JSON preserves that reference. — GPT-5
- **Render intent whole-frame overrides.** The render graph compiler now applies
  `selector: all` view overrides to the frame intent before synthesizing graph
  nodes, keeping graph topology compiler-owned. — GPT-5
- **Render graph DOT exports.** Resource labels in DOT exports now include the
  resource format and lifetime. — GPT-5
- **Modeler graph trace empty states.** Resource trace previews now distinguish
  undeclared resources from declared resources that were not read or written in
  the last execution. — GPT-5
- **Modeler render graph pass tooltips.** Pass nodes in the Render Graph dock
  now summarize reads, writes, and dependency edges on hover. — GPT-5
- **Render graph DOT exports.** Pass labels in DOT exports now include computed
  execution stage and serial order metadata. — GPT-5
- **Render graph text exports.** Per-pass text export details now include the
  computed execution stage and serial order. — GPT-5
- **Modeler render graph dependency properties.** Selecting a pass node now
  shows its incoming and outgoing dependency edges in the property editor.
  — GPT-5
- **Modeler render graph pass properties.** Selecting a pass node now shows its
  execution stage and serial order in the property editor. — GPT-5
- **Modeler render graph resource tooltips.** Resource nodes in the Render Graph
  dock now summarize producer and consumer pass edges on hover. — GPT-5
- **Modeler render graph resource labels.** Resource nodes in the Render Graph
  dock now show format, lifetime, and size directly in the graph view.
  — GPT-5
- **Modeler render graph node labels.** Pass nodes in the Render Graph dock now
  show their execution stage and serial order directly in the graph view.
  — GPT-5
- **Modeler render graph pass stages.** The Render Graph dock's Passes tab now
  shows each pass's dependency-ready execution stage separately from its serial
  execution order. — GPT-5
- **Modeler render graph trace summaries.** After a traced preview render, graph
  nodes now show pass execution status/timing and resource preview/cache status
  directly in the graph view. — GPT-5
- **Render graph shadow cache invalidation.** Raster preview shadow depth cache
  keys now use a pass-specific scene/camera/light fingerprint, so display-only
  changes such as tonemap swaps do not invalidate cached shadow artifacts.
  — GPT-5
- **Modeler render graph live highlighting.** The graph view now waits until a
  pass has been executing for roughly 500 ms before showing live running state,
  so camera movement, frame scrubbing, and resize-triggered preview renders no
  longer flicker through short pass updates. — GPT-5
- **Modeler graph trace preview sizing.** Small trace images in the central
  Graph Trace preview are now scaled up for inspection. — GPT-5
- **Modeler graph trace placement.** The central Graph Trace preview is now the
  only trace image UI; the Render Graph dock no longer has a separate Trace tab.
  — GPT-5
- **Full-resolution render graph trace snapshots.** Color snapshots recorded in
  execution traces now preserve the full graph resource resolution instead of
  being downsampled. — GPT-5
- **Opt-in render graph trace capture.** `GraphRenderEngine` no longer records
  execution traces by default; the Modeler graph inspector enables tracing for
  preview inspection, and rendercli enables it only when
  `--render_graph_trace_out` is requested. — GPT-5
- **Render graph stale-trace filtering.** Modeler now ignores completed graph
  traces whose executed plan no longer matches the inspector's effective plan,
  preventing old snapshots from attaching to matching pass ids after resize,
  graph override, or pass-state changes. — GPT-5
- **Render graph dependent culling.** Applying graph overrides to a
  `CullDependents` pass now disables downstream consumers transitively before
  validation, so graph inspection can show the effective culled subgraph instead
  of reporting the first disabled dependency as an error. — GPT-5
- **rendercli graph default.** `rendercli` now renders through the compiled
  render graph by default, keeps `--render_graph` as an explicit spelling of the
  default path, and adds `--direct_engine` / `--no_render_graph` for focused
  single-engine debugging. — GPT-5
- **Rasterizer tiled MSAA scratch reuse.** Queued MSAA rasterization now keeps tile-indexed color, depth, accumulation, and stencil scratch buffers across frames, reuses the full-frame MSAA sample buffer across samples, and writes packed RGB output by tile when the rendered frame used multiple tiles. — GPT-5
- **Rasterizer tile binning now rejects projected-triangle tiles outside coverage.** Large triangles spanning many tile bounds are queued only for tiles whose expanded tile rectangle can intersect the triangle, reducing repeated empty raster work while preserving tiled/MSAA output. — GPT-5
- **Rasterizer scene traversal now uses grouped bounds for frustum culling.** `Primitive::forEachLeafInBounds(...)` lets composite scenes reject off-frustum groups before leaf flattening and tessellation, while primitives without grouped traversal keep the existing leaf-walk behavior. — GPT-5

### Fixed

- **SSE3 vector dot products.** The SSE3-specialized vector dot products now
  keep public construction and setter paths in SIMD storage before reducing
  lanes, avoiding union type-punning undefined behavior without leaving the hot
  path. — GPT-5
- **Direct group-root imports in rendercli.** Importers that return a grouped
  asset root now receive shared scene defaults and camera framing in direct
  rendercli/modeler opens, so STL, 3MF, and glTF group imports render without a
  separate scene wrapper. — GPT-5
- **Modeler material selector visibility.** Reference properties such as surface
  and source-asset materials now keep their combo-box selector visible in the
  compact property editor instead of collapsing to an empty searchable row. —
  GPT-5
- **Modeler preview camera preservation.** Editing scene properties now refreshes
  preview geometry and materials without snapping the live preview camera back
  to the saved scene camera; opening a new scene still resets to that scene's
  camera. — GPT-5
- **Generated mesh degenerate triangles.** Imported/generated mesh primitives now
  skip zero-area triangles and fall back to geometric flat normals when source
  normals are zero, preventing OpenSCAD-generated STL imports from crashing
  Modeler on load. — GPT-5
- **OpenSCAD crash diagnostics.** Failed OpenSCAD subprocesses now report that
  the tool crashed or failed to start instead of presenting an opaque exit code
  `-1`. — GPT-5
- **Qt autogen after pulls.** The `raytracer` target now makes automoc depend
  on the expanded source/header/UI glob lists, so existing build trees regenerate
  moc sources for newly pulled `Q_OBJECT` headers such as `SourceAsset`. —
  GPT-5
- **Rasterized LDraw instance materials.** The rasterizer now preserves nested
  materials while traversing instanced imported geometry, so LDraw subfile
  instances render with LEGO colors instead of diagnostic fallback face colors;
  repeated instanced leaf tessellation is also cached per frame. — GPT-5
- **Direct LDraw model rendering.** Direct `rendercli --ldraw_input` now
  resolves standard backslash subpart references, loads `LDConfig.ldr` colors
  from the library root, skips empty missing-part placeholders without poisoning
  scene bounds, frames the generated camera around imported geometry, and
  defaults to LDraw-to-renderer coordinate conversion so standard MPD models do
  not render as a uniform background. — GPT-5
- **rendercli step ghost playback.** `rendercli --step_ghost_previous` now
  applies cumulative step visibility before styling, so previous-step groups
  remain visible as ghosts instead of being filtered out by the active-step
  selection. — GPT-5
- **Modeler render graph compile errors.** The Render Graph dock now shows
  graph compilation failures and pauses the preview instead of letting
  unsupported render intent, such as selector-specific overrides, escape the
  preview refresh path. — GPT-5
- **Modeler graph preview scene switching.** Opening a scene without saved
  render intent now resets Modeler preview controls to the default graph intent,
  preventing stale view modes such as stencil composite from leaking from the
  previously opened scene into the render graph dock. — GPT-5
- **Render graph external inputs.** Graph execution now rejects plans that read
  unbound imported or history resources instead of allocating empty placeholders
  for those inputs. — GPT-5
- **Render graph resource-domain validation.** Plan validation now rejects
  current CPU-backed passes that read or write GPU-domain resources, making
  replayed future-domain graph JSON fail before execution. — GPT-5
- **Render graph selector-specific intent.** Graph compilation now rejects
  selector-specific render-intent overrides with a clear error instead of
  silently ignoring targeted scene intent until scene partitioning is supported.
  — GPT-5
- **rendercli direct raytracer scene cameras.** The direct raytracer path now
  installs its tiled render view plane after selecting the scene camera, avoiding
  crashes when small CTest renders exercise the raytracer with many tiles.
  — GPT-5
- **rendercli unknown samplers.** `rendercli --sampler` now rejects unknown
  sampler names with a command-line error instead of dereferencing a missing
  factory result during rendering. — GPT-5
- **Render graph shading parameter parsing.** `ShadingProfileParameterValue::fromText(...)`
  now trims surrounding whitespace before parsing bool and numeric values. — GPT-5
- **Render graph exported-AOV validation.** `RenderIntent` now rejects
  non-AOV view modes in `exportedAOVs` at the intent boundary instead of
  allowing invalid scene JSON to fail later during graph compilation. — GPT-5
- **Group-aware render visibility.** Hidden world-scene groups now suppress all
  descendant surfaces, lights, and nested groups during runtime scene
  conversion, while visible groups preserve child visibility flags consistently
  across raytracer, rasterizer, and wireframe renders. — GPT-5
- **rendercli whole-frame graph overrides.** `rendercli` now applies
  scene-authored `selector: all` render-intent overrides before choosing graph
  sampling and executor-specific pass state, so raster MSAA/shadow/postprocess
  settings are preserved when a scene override switches the synthesized graph
  to rasterizer. — GPT-5
- **Render graph exported-resource validation.** Plans now reject exported
  resources that have no declared producer, so malformed graph-only plans fail
  before execution. — GPT-5
- **Raster graph AOV geometry.** Rasterizer-backed graph AOVs for depth,
  normal, object-id, material-id, and world-position now use rasterizer
  diagnostic buffers, so they match tessellated raster geometry and raster pass
  state instead of analytic raytraced intersections. — GPT-5
- **Metadata-only graph trace resources.** Execution traces now ask runtime graph
  resources whether a CPU depth buffer exists before capturing a depth preview,
  so GPU/metadata-only shadow resources stay inspectable instead of throwing
  during Modeler Graph Trace preview updates. — GPT-5
- **Render graph trace materialization.** LDR graph preview renders now keep
  color resources materialized for execution traces instead of fusing the
  default beauty/tonemap chain into a packed-display-only path, so the Modeler
  Graph Trace preview can inspect input and output images after a render.
  — GPT-5
- **Modeler invalid graph-node toggles.** Disabling a required Render Graph
  node in the Modeler inspector now pauses live preview rendering instead of
  starting an invalid graph on the worker thread, and render-thread exceptions
  are reported through `RenderWidget` instead of aborting the process. — GPT-5
- **Modeler graph node spacing.** The Render Graph dock now leaves a full
  resource lane between pass ranks, preventing resource nodes such as
  `beauty_color` from being hidden under adjacent pass boxes. — GPT-5
- **Raster preview shadows for unmapped lights.** Rasterizer shadow-enabled
  graph renders now trace direct-light visibility for lights that do not have a
  directional shadow-map resource, so point-lit Modeler previews and render
  dialog outputs no longer stay fully lit when shadows are enabled. — GPT-5
- **Modeler AA graph wiring.** Preview FXAA/SMAA now compile through shared
  `RenderIntent::postProcessAA` graph nodes for every live preview executor,
  and the Modeler raster render dialog now renders through the same graph path
  so post-AA and shadow-map settings are not hidden in a separate direct-engine
  setup. Choosing rasterizer preview shadows still switches the live preview to
  Rasterizer before recompiling the graph, so the Render Graph dock immediately
  shows the requested shadow node. — GPT-5
- **SMAA preview visibility.** The CPU SMAA approximation now detects diagonal
  edge axes in addition to horizontal and vertical edges and uses a stronger
  edge blend, making the Modeler raster preview visibly change when SMAA is
  selected. — GPT-5
- **Render graph dependency execution.** `GraphRenderEngine` now executes valid
  plans in resource dependency order instead of pass declaration order, so
  replayed JSON can describe graph edges without hand-sorting the pass list
  while validation still catches missing, disabled, duplicate, and cyclic
  dependencies. — GPT-5
- **Render graph passthrough validation.** Disabled `Passthrough` passes now
  validate that they have one input and shape-compatible outputs before
  execution, turning malformed graph JSON into an inspection-time validation
  error instead of a render-time exception. — GPT-5
- **ACES tonemap invalid-channel handling.** `AcesTonemap` now maps negative
  and NaN HDR channels to black and positive infinity to white before applying
  the polynomial fit, preventing invalid radiance from turning Modeler previews
  into flat gray/white frames. — GPT-5
- **Rasterizer DOF camera fallback.** `ThinLensCamera` and `TiltShiftCamera`
  now expose pinhole-compatible forward projection, so Rasterizer and Wireframe
  previews render scene geometry for DOF scenes instead of producing empty
  output. Raytraced depth-of-field and tilt-shift effects are unchanged. — GPT-5
- **Modeler graph preview updates progressively again.** Simple graph-backed
  LDR previews now use the wrapped beauty engine's display-buffer render path
  when the effective graph is a beauty pass plus optional tonemap pass, so the
  preview widget can publish partial pixels while the frame is still rendering
  instead of waiting for final graph composition. — GPT-5
- **Postprocess graph previews keep raytracer progress.** Graph-backed LDR
  previews with downstream postprocess passes now render raytracer beauty into
  the HDR graph resource and packed display buffer in one tile pass, so
  FXAA/SMAA no longer make the Modeler preview wait for the whole raytraced
  frame before showing partial output. — GPT-5
- **Modeler graph preview startup crash.** `QtDisplay` now initializes its backing buffer after its default widget resize, and the Modeler graph preview compiles plans against that backing-buffer size, preventing graph color-resource copies from aborting on startup due to stale dimensions. — GPT-5
- **Textbook image assets now publish with the static HTML build.** `rake docs:textbook:html` copies `docs/images` into `docs/html/textbook/images`, so rendered PNGs and graph SVG artifacts resolve when the textbook is served locally. — GPT-5
- **Wireframe now clips edges crossing the near plane instead of dropping them.** `Wireframe::nearClipDepth()` defaults to `0.1`; edges with one endpoint behind that depth are shortened before projection, while edges fully behind it are skipped. — GPT-5
- **Rasterizer coverage now preserves subpixel projected vertex positions.** Projected screen coordinates remain fractional until fixed-point edge setup, reducing whole-pixel edge jumps during small camera or object motion while keeping tiled and MSAA paths consistent. — GPT-5
- **Rasterizer shared triangle edges no longer shade twice.** `core::rasterizeTriangleSampled` now uses a top-left fill rule for samples exactly on triangle edges, so adjacent triangles covering one surface assign shared-edge pixels to only one triangle and avoid double-applying stencil operations. — GPT-5
- **Modeler timeline preview now starts from animated scene cameras.** Changing timeline frames rebuilds the central preview camera from the evaluated scene's active camera, then keeps the viewport interactive so modeler camera controls continue from that keyed pose. — GPT-5
- **Rasterizer shadow-map projections no longer drift by fractional texels during small camera moves.** Directional shadow-map and cascade centers are now snapped to their light-space texel grid before the depth pass, reducing preview shimmer while preserving the existing shadow-map size, bias, filtering, and cascade controls. — GPT-5
- **Benchmark preset builds no longer fail in `BVHPacketBenchmark`.** Packet benchmark ray groups now initialize their non-default-constructible `Rayd` arrays explicitly, and packet results are passed to Google Benchmark's non-deprecated `DoNotOptimize` overload. — GPT-5
- **Raytraced instanced objects no longer warp when their transform makes `Matrix4::inverted()` choose a near-singular block pivot.** The 4×4 block inverse now falls back to the cofactor path for near-zero bottom-right 2×2 determinants, fixing rotated/scaled torus intersections and their shadow rays. — GPT-5
- **Clang release builds no longer fail on render primitive override warnings.** Render primitive headers now mark their `Primitive`/`Composite` overrides explicitly, keeping `-Werror -Winconsistent-missing-override` builds green. — GPT-5
- **Rasterizer and Wireframe now honor the scene's `background()`.** Previously both engines kept a private `m_backgroundColor` field defaulted to black and ignored the scene's background — any scene loaded into `rendercli --engine raster` or `Modeler`'s preview rendered against black regardless of what the scene specified. Background handling moved to `render::RenderEngine` as a single shared override-plus-fallback: `setBackgroundColor`/`clearBackgroundColor`/`hasBackgroundColorOverride` live on the base, and the virtual `backgroundColor()` returns the override if set, otherwise the scene's `background()`, otherwise black. Rasterizer inherits the default; Wireframe overrides to fall back to black (preserving its lines-on-black look). Raytracer is unaffected (it already read the scene's background directly for miss rays). — Claude Opus 4.7

### Changed

- **Rasterizer culling defaults now follow material sidedness.** Front-sided materials default to backface culling, back-sided materials default to frontface culling, and two-sided materials keep both faces unless the caller explicitly sets a rasterizer cull mode. — GPT-5
- **Rasterizer UV texture sampling avoids unnecessary hit-context fabrication.** The built-in raster material path now samples exact `UVColorTexture` and UV-mapped `CheckerBoardTexture` albedos directly from interpolated fragment UVs, while arbitrary texture subclasses still use virtual `Texture::evaluate(...)` with a synthesized hit context. — GPT-5
- **Rasterizer built-in material shading now previews local Phong terms.** The raster material path now honors Matte ambient/diffuse coefficients and Phong specular color, coefficient, and exponent for local direct lighting; recursive reflection and refraction remain raytracer-only. — GPT-5
- **Rasterizer culls off-frustum finite primitive bounds before tessellation.** The raster front end now rejects leaf primitives whose finite bounding boxes are wholly outside one homogeneous clip plane before building their meshes, while keeping infinite/invalid bounds and custom vertex-shader camera passes conservative. — GPT-5
- **Rasterizer shadow cascades now use practical split blending.** Multi-cascade directional shadow maps expose a 0-to-1 linear/logarithmic split blend across the C++ API, rendercli, and Modeler settings, defaulting to 0.5 so near camera depths receive more stable shadow-map detail without giving up far coverage. — GPT-5
- **Rasterizer directional shadow cascades now fit light-space bounds.** Directional shadow-map cameras measure each camera-depth slice in the light's own basis instead of sizing the map from the world-space cascade diagonal, giving cascades denser texel coverage while preserving texel snapping and existing cascade controls. — GPT-5
- **Rasterizer shadow-map visibility is pre-bound per pass.** The built-in raster material evaluator now prepares each scene light with its radiance and optional directional shadow map before fragment shading, avoiding per-fragment searches through the shadow-map collection. — GPT-5
- **Rasterizer shadow-map depth passes now skip color scratch buffers.** Directional shadow maps use a true depth-only raster pass, preserving the same shadow visibility behavior while avoiding unused color allocation and writes during light-space rendering. — GPT-5
- **Modeler promoted out of `examples/`.** The interactive scene editor now builds as the `Modeler` target from `src/modeler`, reusable JSON scenes live under top-level `scenes/`, and the legacy `examples/` applications have been removed. — GPT-5
- **Rasterizer default 1× previews now stream triangles directly.** The ordinary `queueSize == 1`, `msaa == 1` path skips `RasterTriangleSet` materialization and tile binning, freezing depth/stencil/fragment policies once and rasterizing emitted triangles immediately into the full-frame pass buffers; tiled rendering and MSAA still retain the prepared triangle set they need for tile ownership and repeated samples. Before/after PNGs stayed byte-identical. Median `rendercli --repeat` timings moved from `18.760 ms → 16.829 ms` on `rasterizer_baseline_materials.json` 640×480 LOD 3, `104.814 ms → 100.461 ms` on `rasterizer_baseline_offscreen_floor.json` 1920×1080 LOD 0, and `4194.552 ms → 1841.772 ms` on `rasterizer_baseline_dense_sphere.json` 640×480 LOD 8. — GPT-5
- **Rasterizer tiled MSAA now uses tile-local sample buffers.** Queued MSAA renders (`queueSize > 1`) now render all samples for a tile inside one task, using tile-local color/depth/stencil buffers and resolving directly into the output framebuffer; the default `queueSize == 1` path keeps the specialized full-frame MSAA loop. Before/after PNGs stayed byte-identical. `rasterizer_baseline_materials.json` 640×480 LOD 3 4× MSAA measured `64.660 ms → 59.718 ms` median by default and `38.918 ms → 20.236 ms` median tiled; `rasterizer_baseline_offscreen_floor.json` 1920×1080 LOD 0 4× MSAA measured `454.027 ms → 436.677 ms` median by default and `231.608 ms → 95.671 ms` median tiled. — GPT-5
- **Rasterizer built-in material shading now prepares per-primitive albedo evaluators.** Matte/fallback material classification moved out of the fragment loop; exact `ConstantColorTexture` albedos are cached per emitted triangle, while arbitrary textures still receive the interpolated `HitPoint`/`Ray` context. On `rasterizer_baseline_materials.json` at 640×480 LOD 3, byte-identical output measured `21.602 ms → 20.688 ms` median for 1× and `69.370 ms → 63.474 ms` median for 4× MSAA (`rendercli --repeat 10`). — GPT-5
- **Replace Meyer's-singleton static factories with `inline` variables on all math types (modernize.md §3.1, item 1a).** `null()`, `one()`, `epsilon()`, `undefined()`, `minusInfinity()`, `plusInfinity()` on `Vector2/3/4<T>` and their SSE3 specializations — plus `Ray::undefined()` and `BoundingBox::undefined()`/`infinity()` — converted from Meyer's-singleton functions to `inline const` (SSE3 types) or `inline constexpr` (generic templates) static data members. All ~258 call sites updated; parentheses removed. `HitPoint::undefined()` is out of scope and preserved. — Claude Sonnet 4.6
- **`Matrix4 * Matrix4` and `Matrix4 * Vector4` SIMD specializations (roadmap §2.1).** SSE float specialization for `Matrix4<float>::operator*(Matrix4<float>)` uses the "column broadcast" approach: load all 4 rows of B once, then for each row of A broadcast each scalar element via `_mm_shuffle_ps` and accumulate with `_mm_mul_ps`/`_mm_add_ps` — 4 SIMD mul+add steps vs. 64 scalar ops. SSE float mat-vec uses element-wise row-vector products followed by a 4×4 in-place transpose (`_MM_TRANSPOSE4_PS`) and a 2-level add tree. SSE2 double specializations use two `__m128d` registers per row with `_mm_unpacklo/hi_pd` for broadcasting (matmul) and per-row horizontal dot products (mat-vec). Both specializations hook in via explicit member-function specialization in new headers `include/core/math/matrix/sse2/Matrix4f.h` and `include/core/math/matrix/sse2/Matrix4d.h`, included at the bottom of `Matrix.h` in the same pattern as the `vector/sse3/` headers. Before/after benchmark numbers are in the PR description. Closes roadmap §2.1. — Claude Sonnet 4.6

- **`Matrix4::transformPoint` / `transformDirection` affine fast paths (roadmap §2.5).** Two new `Matrix4<T>` methods skip the homogeneous bottom row and (for directions) the translation column, reducing multiply-adds from 16 to 12 (`transformPoint`) and 9 (`transformDirection`) vs. the full `Matrix4 * Vector4` path. All `Instance` transform call sites updated: bounding-box vertex loop, tessellation, `farthestPoint`, and the motion-blur ray-origin path. `PortalMaterial::transformedRay` also updated. Benchmarks `bm_transform_point` / `bm_transform_direction` added to `MatrixBenchmark.cpp`; six new correctness tests pin bit-identical results against the homogeneous path and verify translation-handling for each method. Closes `docs/plans/core-math-optimization.md` §2.5. — Claude Sonnet 4.6
- **`constexpr` / `noexcept` / `[[nodiscard]]` sweep across all math headers (modernize.md §3.1).** Annotated every applicable method in `Vector.h`, `Matrix.h`, `Ray.h`, `BoundingBox.h`, `Quaternion.h`, and `Range.h`: pure arithmetic operators and getters are now `constexpr noexcept`; functions calling `std::sqrt`/`std::abs`/trig that cannot throw are `noexcept`; all value-returning functions carry `[[nodiscard]]`. SSE3 specializations under `include/core/math/vector/sse3/` receive `noexcept` and `[[nodiscard]]` (SIMD intrinsics are not constexpr-eligible). `operator/` and paths that call it remain non-noexcept (they throw `DivisionByZeroException`). A compile-time `static_assert` block in `VectorTest.cpp` proves constexpr evaluation at compile time for `Vector3<int>` arithmetic. — Claude Sonnet 4.6

- **`Polynomial::solve()` CRTP refactor (roadmap §2.4).** `Polynomial<T,N>` is now a CRTP base `Polynomial<T,N,Derived>`; `solve()` is no longer virtual. `solveInto()` and `sortedResult()` dispatch to `Derived::solve()` via `static_cast`, eliminating the vtable pointer and indirect call on the torus inner loop where the polynomial degree is compile-time-known. — Claude Sonnet 4.6
- **`Matrix4::inverted()` replaced cofactor expansion with block-inverse (Schur complement).** Partitions the 4×4 into four 2×2 blocks, inverts D and the Schur complement S = A − B·D⁻¹·C, and assembles the four result blocks — ~60 explicit multiplications vs ~120 in the old cofactor path. Benchmark: 41 ns → 20 ns (float), 39 ns → 17 ns (double) on a TRS matrix; 2.0–2.4× speedup consistently across runs. Falls back to the full cofactor expansion when the bottom-right 2×2 block is singular (det D = 0), preserving correctness for any invertible 4×4 (e.g. camera view matrices with a horizontal view direction). Four numerical-stability tests verify correctness. Closes core-math-optimization.md §2.2. — Claude Sonnet 4.6
- **`Polynomial::sortedResult()` now returns stack-allocated `SortedResult<T, N>` instead of `std::vector<T>`.** Eliminates a per-ray heap allocation on the torus intersection hot path (baseline: `bm_quartic_sorted_result` ~160 ns vs `bm_quartic_solve_into` ~70 ns; gap closes to near parity after this change). Torus intersection updated to use `auto`; test helper updated to compare heterogeneous container types. — Claude Sonnet 4.6
- **`HitPointInterval` small-buffer optimization (Phase 1.4).** Replaced the `std::vector<HitPointWrapper>` backing store with `SmallVector<HitPointWrapper, 4>` — a fixed-capacity inline buffer that holds up to 4 hit points on the stack and falls back to heap only for deeper CSG scenes. Intervals with ≤4 hit points (the common case: sphere, plane, box, triangle) now incur zero heap allocations per ray. Measured ≥39% speedup on the 1–2-hit path and ≥35% on the 4-hit path on Linux/x86-64 at `-O3`; closes roadmap Phase 1.4. — Claude Sonnet 4.6
- **`BoundingBox::intersects(Ray)` rewritten as branchless SIMD slab method.** The generic template replaces per-axis sign branches with `std::min`/`std::max` (compiler emits conditional moves, no mispredictions). An SSE2 explicit specialization for `BoundingBox<double>` — the hot path in BVH traversal — computes the X and Y axes in one `__m128d` pass via `_mm_div_pd`/`_mm_mul_pd`/`_mm_min_pd`/`_mm_max_pd`. New `intersect(Ray, Range<T>&)` overload populates the `[t_enter, t_exit]` interval so BVH children can be sorted by entry time without recomputing the slab math. Measured speedup on this build host: ~1.4–1.5× for `BoundingBox<double>` (256-ray batch: 222 M/s → 302–342 M/s; 10k-ray batch: baseline est. ~220 M/s → 295–330 M/s). The 3× plan target was not met on the 2.5 GHz VM; higher-frequency hardware is expected to show larger gains. Benchmark added: `bm_intersects_batch` (10k rays) and `bm_intersect_interval`. Six new unit tests cover interval correctness. Closes roadmap §core-math optimization plan §1.2. — Claude Sonnet 4.6
- **`Number::random` and `Range::random` now use a thread-local PCG32 PRNG** instead of `std::rand`. Removes the global lock and poor statistical quality; single-thread throughput improves from ~6.3 ns/call (baseline `docs/perf/math-baseline-2026-05-10.txt`) and multi-thread throughput scales linearly with thread count. New free function `seed(uint64_t)` seeds the calling thread's PRNG for deterministic tests. The `random_shuffle` utility in `include/core/util/Random.h` uses the same per-thread PRNG. Closes `docs/plans/core-math-optimization.md` §1.1. — Claude Sonnet 4.6
- **Delete `Vector3<double>` SSE3 specialization (Phase 2.3).** The two-`__m128d` specialization in `include/core/math/vector/sse3/Vector3d.h` is removed; `Vector3<double>` now uses the generic template. Three-way benchmark (baseline SSE3 UB, fixed SSE3 via `_mm_hadd_pd`, scalar/autovec) showed: fixing the UB makes dot product 80% slower than the UB version — worse than plain scalar; the AVX2 single-register option is 2× slower on cross product. The scalar path (option A) ties or beats all correct alternatives and unblocks compiler autovectorization at call sites. Decision benchmark at `docs/perf/phase-2.3-vec3d-decision-2026-05-10.txt`. Closes roadmap §core-math-optimization Phase 2.3. — Claude Sonnet 4.6

### Added

- **Triangle, Plane, Box, MeshTriangle, and BoundingBox Ray4 packet kernels (Epic #141, Phase 4.1).** Added SSE `intersectPacket(Ray4)` implementations for the remaining hot primitive leaves plus `BoundingBox::intersects4(Ray4)` for future block-batched BVH traversal. New `BatchedRayBenchmark.cpp` compares 256-ray and 10k-ray scalar loops against Ray4 packets for Sphere, Triangle, Plane, Box, and BoundingBox; on this runner the 10k-ray packet paths measured 2.9x to 14.3x faster than scalar loops. — GPT-5

- **Block-batched BVH traversal with active-mask packet path (Epic #141, Phase 4.3).** `BVH::intersectPacket(Ray4)` walks all four lanes through a single tree descent, testing each node AABB against the active mask and recursing only into subtrees with at least one active lane; leaf nodes dispatch to `Primitive::intersectPacket` for cache-friendly SIMD primitive intersection. `BVH::intersectPacket(Ray8)` extends the same algorithm to eight lanes under `__AVX__`. Coherent primary rays (same 2×2 pixel tile) benefit from both tree-level cache reuse and leaf-level SSE parallelism; incoherent rays (random directions) stay within ~20% of scalar cost due to sparse active masks. New `benchmarks/BVHPacketBenchmark.cpp` provides coherent-vs-incoherent comparisons plus a macro primary-render benchmark. — Claude Sonnet 4.6

- **Stable quartic fallback for ill-conditioned polynomial solves (core-math optimization Phase 4).** `Quartic` now exposes an explicit `solveStable()` / `stableSortedResult()` path backed by `StablePolynomial`, which isolates real roots through derivative-split monotone intervals and bisection. Torus intersection selects that path when coefficient magnitudes are poorly scaled, preserving grazing-incidence hit intervals without slowing the normal Ferrari `Quartic::solve()` benchmark path. — GPT-5
- **Ray4/Ray8 SoA ray packets and Sphere packet intersection (Epic #141, Phase 4.1).** Added aligned `Ray4`/`Ray8` origin/direction lane storage with extraction back to scalar rays, packet hit-mask/distance results, `Primitive::intersectPacket` fallback entry points, and an SSE `Sphere::intersectPacket(Ray4)` kernel. `RayPacketBenchmark` compares the new Ray4 sphere path to four scalar sphere intersections: on this runner, scalar loop measured `9.40M rays/s` and Ray4 measured `22.9M rays/s` (~2.4x). — GPT-5
- **Matrix decompositions (core-math optimization Phase 4.4).** Added LU partial pivoting, QR orthonormalization, and SVD as `Matrix` instance methods for small `Matrix3`/`Matrix4` use cases, plus `Matrix4::stableInverse()` which keeps the existing block inverse on well-conditioned matrices and recomputes ill-conditioned inverses with LU. Unit tests pin canonical LU/QR/SVD reconstructions and stable-inverse residuals; `MatrixBenchmark.cpp` now tracks the stable-inverse fast path. Closes `docs/plans/core-math-optimization.md` Phase 4 matrix decompositions. — GPT-5

- **`Matrix4` view/projection static factories and camera adoption (roadmap §3.5).** Added `Matrix4::lookAt`, `Matrix4::perspective`, `Matrix4::orthographic`, and `Matrix4::frustum` as `[[nodiscard]] static` factories on `Matrix4<T>`. `Camera::matrix()` already delegates to `lookAt`; `PinholeCamera` and `OrthographicCamera` each gain a `projectionMatrix()` method that builds the corresponding `frustum`/`orthographic` matrix, and their `projectPointToClipSpace` implementations now route x/y through it instead of carrying inline perspective math. 11 new unit tests cover all four factories against canonical NDC values and verify that `projectionMatrix()` is consistent with `projectPointToClipSpace`. — Claude Sonnet 4.6

- **`std::hash` and `std::formatter` specializations for all math types (roadmap §3.6).** `std::hash` added for `Vector2/3/4<T>`, the generic `Vector<Dimensions,T,StorageCellType,Derived>` base, `Matrix2/3/4<T>`, `BoundingBox<T>`, and `Quaternion<T>` — all using the Boost-style hash-combine mixing function. Enables `unordered_map`/`unordered_set` keys, e.g. vertex deduplication via `unordered_map<Vector3d, int>`. `std::formatter` specializations (same types) are compiled in when the toolchain provides C++20 `<format>` (`__cpp_lib_format`); on C++17 toolchains the existing `operator<<` overloads serve as the fallback. — Claude Sonnet 4.6

- **Complete `Quaternion` class (Phase 3.3).** Added `conjugate`, `inverse`, `dot`, `lengthSquared`, `operator+`, `operator-`, `rotate(Vector3)`, `fromAxisAngle`, `fromEulerAngles`, `toEulerAngles`, `toMatrix3`, `toMatrix4`, `nlerp`, and `slerp` to `include/core/math/Quaternion.h`, growing it from ~90 to ~250 lines. `QuaternionBenchmark.cpp` extended to cover all new ops (32 benchmarks total). Unit tests pin all key identities including `q * q.conjugate() ≈ I` and Euler round-trip. — Claude Sonnet 4.6
- **`Constants.h` inline constexpr + missing constants.** All four existing math constants (`PI`, `TAU`, `invPI`, `invTAU`) converted from `const double` to `inline constexpr double`; eight new constants added: `PI_OVER_2`, `PI_OVER_4`, `DEG_TO_RAD`, `RAD_TO_DEG`, `SQRT2`, `SQRT3`, `E`, `GOLDEN_RATIO`. A comment flags the C++20 `<numbers>` migration path (roadmap §3.1). All existing call sites compile unchanged. Closes core-math-optimization plan §3.7. — Claude Sonnet 4.6
- **Throughput-based adaptive recursion cutoff in `Raytracer::rayColor`.** `render::State` gains a `throughput` field (default `1.0`) that tracks the accumulated path weight (product of reflection/transmission attenuation along the bounce chain). `ReflectiveMaterial`, `TransparentMaterial`, and `PortalMaterial` update `state.throughput` before each recursive `rayColor` call and restore it afterward. `Raytracer::rayColor` short-circuits to the scene background when `throughput` drops below `RAYTRACER_THROUGHPUT_CUTOFF` (1e-4, added to `Constants.h`), eliminating invisible deep-recursion tails with a bounded introduced bias of at most `ε × max_radiance`. The existing `setMaximumRecursionDepth` hard limit is preserved as an orthogonal upper bound. — Claude Sonnet 4.6
- **Camera aspect-ratio fit modes.** `render::AspectMode` (`Stretch`, `FitWidth`, `FitHeight`, `FitExact`) on `Camera` and `ViewPlane` controls how the rendered view rectangle relates to the framebuffer when their aspect ratios differ. `FitWidth` (the new default for `RenderWidget` and `QtDisplay`) keeps horizontal FOV constant and derives vertical from the buffer shape, ensuring square pixels and no geometric distortion on resize. `FitExact` adds letterbox/pillarbox black bars so a fixed intrinsic aspect ratio is always preserved. `Stretch` is the pre-existing behavior (hard-coded 4:3 world extents, no geometric guarantee). `Modeler` exposes a mode selector and aspect-ratio presets in the Render menu. Fixes the "renders squish when the preview window resizes" bug. — Claude Sonnet 4.6
- **Vector math helpers: `reflect`, `refract`, `lerp`, `clamp`, `saturate`, `cwiseMin`, `cwiseMax`, `approxEqual`, structured bindings.** Added to the generic `Vector` base template in `include/core/math/Vector.h`; all types (including SSE3 specializations) inherit the ops. C++17 structured-bindings support enables `auto [x, y, z] = v` for Vector2, Vector3, and Vector4. `PerfectSpecular` now uses `(-out).reflect(n)` and `PerfectTransmitter` now uses `out.refract(n, eta)` — open-coded duplicates removed. `VectorBenchmark.cpp` extended with `bm_reflect`, `bm_refract`, `bm_lerp`, `bm_clamp`, `bm_saturate`, `bm_cwise_min`, `bm_cwise_max`. Closes `docs/plans/core-math-optimization.md` §3.4. — Claude Sonnet 4.6

- **Syrus-native CI via `.syrus.yml`.** Two graders run on every implement→grade iteration: `build-test` (`cmake --preset release` + parallel ctest under xvfb) and `textbook` (markdown drift + source-map appendix freshness). Single compiler (clang-18); single Linux x86_64 worker (K3s on Intel NUC i7 gen 12). Parked all `.github/workflows/*.yml` and `.github/dependabot.yml` under `docs/plans/github_actions/` with a README explaining the migration scope and what's awaiting Syrus features. — Claude Opus 4.7
- **`Matrix4` camera and projection factories.** `Matrix4::lookAt(eye, target, up)` builds the camera-to-world transform; `Matrix4::perspective(fovY, aspect, near, far)`, `Matrix4::orthographic(left, right, bottom, top, near, far)`, and `Matrix4::frustum(left, right, bottom, top, near, far)` provide the standard projection matrices (+Z forward, z_ndc ∈ [−1, 1]). `Camera::matrix()` is now routed through `lookAt`, removing the open-coded duplicate; the refactor also fixes a latent correctness bug where the `right` vector was not normalized for cameras not looking horizontally. Closes roadmap §3.5. — Claude Sonnet 4.6
- **Core-math benchmark surface and optimization plan.** Six new Google Benchmark suites (`MatrixBenchmark`, `BoundingBoxBenchmark`, `PolynomialBenchmark`, `HitPointIntervalBenchmark`, `RandomBenchmark`, `QuaternionBenchmark`) plus an expanded `VectorBenchmark` (cross/normalize/sub/scalar mul-div/reflect-chain/dot-batch across Vector3f/3d/4f/4d). Baseline output saved at `docs/perf/math-baseline-2026-05-10.txt` against commit `a064505`. The phased optimization plan with per-step benchmark gates lives at `docs/plans/core-math-optimization.md`. — Claude Opus 4.7
- **Rasterizer FXAA post-process anti-aliasing.** `render::postprocess::applyFxaa` adds a reusable image-space edge filter, `engine::raster::Rasterizer` exposes `setPostProcessAA`, and both `rendercli --engine raster --post_aa fxaa` and the Modeler render dialog expose it as a fast preview AA option alongside MSAA. — GPT-5
- **Rasterizer directional shadow maps.** `engine::raster::Rasterizer` can now opt into directional-light shadow maps for the built-in Lambertian path, with configurable map size and depth bias; `rendercli --engine raster` exposes the same controls through `--shadow_maps`, `--shadow_map_size`, and `--shadow_bias`. — GPT-5
- **Rasterizer PCF shadow filtering.** Directional shadow maps now support configurable percentage-closer filtering through `Rasterizer::setShadowFilterRadius` and `rendercli --engine raster --shadow_filter_radius`, preserving hard shadows at the default radius 0. — GPT-5
- **Modeler rasterizer shadow controls.** The render dialog now exposes rasterizer shadow maps, shadow-map size, depth bias, and PCF radius alongside the existing MSAA selector. — GPT-5
- **RenderWidget front/back display buffers.** GUI renders now write into a render-thread back buffer while `paintEvent` draws an immutable UI-thread front image; `RenderEngine::activeTiles()` / `completedTiles()` expose tile progress so raytracer previews publish only completed tiles, while engines without progressive LDR output publish the full frame on completion. — GPT-5
- **Modeler display update modes.** The render dialog now exposes `Periodic update`, `Completed tiles`, and `Double buffer` display policies for every engine, with progress indicators as an independent checkbox. The central preview keeps the previous image visible while a new render starts, reuses the previous raytracer display buffer for 16 ms whole-buffer point-interlaced updates, and still cancels/restarts on camera movement; Rasterizer and Wireframe previews use double buffering and defer camera rerenders until the current frame completes. Point-interlaced view planes now choose their coarse starting resolution from the full view plane rather than each worker tile, keeping tiled previews visibly coarse at first. Raytracer cancellation now uses an atomic camera flag with additional sample-loop checks, and opening a file no longer starts a redundant preview render of the empty replacement scene before loading the file. — GPT-5
- **Rasterizer shadow-map documentation.** `engine::raster::Rasterizer` docs now include off/on, resolution, and bias render sweeps plus an interactive widget showing the light-space depth pass and camera-pass depth comparison; the doc-render DSL now emits boolean `rendercli` flags such as `--shadow_maps`. — GPT-5
- **Reusable render tiling and homogeneous clip helpers.** `render::TilePlan` now owns exact framebuffer tile partitioning / pixel-to-tile lookup, `engine::TileRenderTask` owns shared QRunnable tile dispatch, and `render::HomogeneousClipVolume` owns reusable homogeneous clip outcodes and Sutherland-Hodgman clipping with caller-provided vertex interpolation. — GPT-5
- **Primitive leaf traversal helper.** `render::Primitive::forEachLeaf` now walks nested composites and reports each leaf with its inherited effective material, giving rasterizer-style engines a reusable scene traversal path. — GPT-5
- **Functional tests for `MatteMaterial` and `PhongMaterial`** (`test/functional/render/materials/MatteMaterialTest.cpp`, `PhongMaterialTest.cpp`): pin texture-color passthrough, ambient-coefficient linearity, the no-illumination contract, and the Phong specular highlight contrast against Matte under a head-on directional light. Shared regex steps in `test/functional/steps/MaterialSteps.cpp`: `"a matte sphere with a (red\|green\|blue\|white\|black) texture"`, `"a matte sphere with ambient ([\\d.]+) and diffuse ([\\d.]+)"`, `"a phong sphere"` / `"a phong sphere with white specular"`, `"a directional light from \\(...\\)"`, `"a dark scene"`, plus matching color-presence and specular-highlight predicates. — Claude Sonnet 4.6
- **Functional test for `ThinLensCamera` DOF invariant** (`test/functional/render/cameras/ThinLensCameraTest.cpp`): a sphere placed exactly at the focal distance renders with a crisp silhouette (few partial-coverage boundary pixels); the same sphere with the focal plane well beyond it blurs into a wide band of intermediate colors, confirmed by a comparative edge-transition count. New regex steps live in `test/functional/steps/ThinLensCameraSteps.cpp`: `"a thin-lens camera"`, `"a thin-lens camera focused at distance ([\\d.]+)"`, `"a thin-lens camera with aperture radius ([\\d.]+)"`, `"a sphere at distance ([\\d.]+)"`, `"record edge count as S"`, `"edge count should be larger than S"`. Adds `previousEdgeCount` to `EngineFeatureTest` following the `previousObjectSize` pattern. — Claude Sonnet 4.6
- **Color model conversion documentation widget.** `Color` docs now include a static educational widget showing RGB storage alongside HSV and CMYK helper views and conversion entry points. — GPT-5
- **Rasterizer depth/stencil/cull documentation widget.** The `engine::raster::Rasterizer` docs now include an interactive framebuffer/depth/stencil widget showing a mark-then-draw stencil pass, overlapping triangles resolved by depth, and segmented controls for stencil testing and Both/Back/Front culling behavior; `rake docs:html` now invokes the shared widget gallery/copy task after Doxygen generation. — GPT-5
- **Support mapping and GJK educational widget.** The primitive support-function, Minkowski-sum, and GJK simplex documentation now share an interactive widget that shows two convex shapes, their support points, the Minkowski-difference support point, and the simplex evolving toward the origin. JS widget tests pin the new controls and rendered structure. — GPT-5
- **Transparent material refraction widget.** Runtime transparent-material and perfect-transmitter docs now include an interactive Snell's-law widget with draggable incident direction, inner/outer IOR sliders, reflected and refracted branches, critical-angle markers, and total-internal-reflection state. — GPT-5
- **Reflective material recursion widget.** Reflective material and perfect-specular BRDF docs now include an interactive widget showing a draggable incoming ray, draggable surface normal, mirror ray, recursive reflected-bounce tree, and reflection coefficient control. — GPT-5
- **Portal material ray-redirection widget.** Portal material docs now include an interactive widget showing the portal plane, incoming ray, inverse-transformed query origin/direction, and color filter swatch, with draggable source-ray and portal-transform handles plus a segmented filter control. — GPT-5
- **Texture coordinate mapping widget.** Checker board texture docs now include an interactive widget showing the hit point, generated `(s, t)` coordinates, `floor(s) + floor(t)` checker parity, and sampled checker preview for planar vs UV mapping with U/V scale controls. — GPT-5
- **Camera forward-projection widget.** `Camera::projectPointToClipSpace` docs now include an interactive pinhole-vs-orthographic widget with a draggable world point, projected pixel, eye-relative depth, and homogeneous `w` readout; JS tests pin the widget structure and controls. — GPT-5
- **Motion blur time-sampling widget.** `Instance::setVelocity` docs now include an interactive shutter-time diagram with a draggable velocity vector, a time scrubber, regular-vs-stochastic sampling controls, sampled positions, and accumulated ghosted silhouettes; the `world::Surface` velocity docs point to the same runtime explanation. — GPT-5
- **Wide-angle camera mapping widget.** The fisheye, spherical, and equirectangular runtime camera docs now share an interactive image-to-unit-sphere mapping widget with a draggable image point, FOV controls for tunable projections, and callouts for fisheye disc cutoff, spherical partial panoramas, and equirectangular seam/pole stretching. — GPT-5
- **Sampler stream documentation widget.** The runtime sampler docs now embed an interactive widget that compares regular, jittered, and random subpixel patterns across sample counts, and shows why pixel, lens, and shutter-time pulls should use independent stream dimensions instead of reusing one 2D pattern. — GPT-5
- **ViewPlane iteration-order documentation widget.** `ViewPlane` docs now embed an interactive grid that compares row-major, tiled, row interlaced, point interlaced, row shuffled, and point shuffled traversal with a progress slider, making progressive whole-frame coverage visible in the generated docs. — GPT-5
- **Grid DDA traversal documentation widget.** `Grid` docs now include an interactive uniform-grid traversal widget with draggable ray origin/direction handles, density control, entry/current-cell markers, `t_next` readouts, and a visited-cell trail. — GPT-5
- **Mesh triangle interpolation widget.** Runtime triangle docs now include an interactive widget showing how ray-triangle barycentric coordinates are shared by inside tests, UV interpolation, smooth normal interpolation, and rasterizer attributes. — GPT-5
- **Instance transform documentation widget.** `render::Instance` docs now include an interactive widget that shows the world ray transformed into local primitive space and compares geometric, inverse-transpose, and direction-scaled normals under non-uniform scale. — GPT-5
- **Phong and Lambertian BRDF lobe widget.** Runtime docs for `MatteMaterial`, `PhongMaterial`, `Lambertian`, and `GlossySpecular` now include an interactive widget with draggable light/view vectors plus diffuse, specular, and exponent controls, showing Lambertian `n dot l`, view-dependent Phong highlights, and exponent-driven lobe narrowing. — GPT-5
- **CSG hit-interval widget.** `HitPointInterval` docs now embed an interactive ray timeline for union, intersection, and difference set operations with draggable enter/exit endpoints, first-positive-hit highlighting, and difference normal-flip markers; the CSG primitive headers cross-reference the concept. — GPT-5
- **BVH SAH traversal documentation widget.** `render::BVH` docs now embed an interactive widget with draggable primitive AABBs, centroid split candidates, SAH cost bars, the selected split, and a movable traversal ray that shows missed child subtrees being pruned; this completes the educational widget coverage item for Priority 2 BVH splitting and traversal. — GPT-5
- **Portal material world wrapper and rendered docs.** `PortalMaterial` is now available through the `world::` JSON scene graph for `rendercli` / editor scenes, and its class docs include a Raytracer-rendered portal image generated by the standard doc-render pipeline. — GPT-5
- **Rendered docs engine coverage.** Class-level docs now include supported-engine render coverage for `OpenCylinder`, `Disk`, `Triangle`, `Rectangle`, `Torus`, `PinholeCamera`, and `OrthographicCamera`, primitive Wireframe images are generated through the same multi-engine `class_doc` suffix convention, and stale unsuffixed image references in the `world::` wrappers were updated so `rake check:doc-images` passes again. — GPT-5
- **Rendered documentation image gallery.** `rake docs:images` now writes `docs/html/rendered-images.html`, a standalone filterable page containing every rendered documentation image copied from `docs/images` for bulk visual review. — GPT-5
- **Rasterizer perspective UV widget shared controls.** `rasterizer_perspective_uv.js` now uses the shared ES6 widget lifecycle, `FigureSvg`, and `FigureSliderControl`; the right-edge depth remains a scalar slider while the UV grid rendering shares the same container and styling path as the other migrated rasterizer widgets. — GPT-5
- **Rasterizer pipeline widget shared primitives.** `rasterizer_pipeline.js` now follows the same ES6 class/shared-widget primitive pattern as the migrated MSAA and clipping widgets while preserving its three draggable triangle vertices, barycentric readout, and barycentric-vs-UV color control. — GPT-5
- **Farthest-point widgets use explicit angle sliders.** `sphere_farthest_point.js`, `box_farthest_point.js`, and `convex_hull_farthest_point.js` now expose their direction angle through a labeled shared slider instead of the old hidden whole-widget horizontal drag gesture, and their final farthest points are drawn larger for readability. — GPT-5
- **Angle unit widgets use explicit sliders.** The four `angle_from_*.js` widgets now inherit a shared 0–720 degree angle slider from `AngleFromX`, replacing the previous invisible whole-widget drag interaction while keeping each unit-specific label and tick layout; the doubled range makes the non-bijective angle mapping visible. — GPT-5
- **Ray parameter widget uses an explicit slider.** `ray_at.js` now exposes the ray parameter `t` through a labeled shared slider instead of using whole-widget horizontal dragging, and the evaluated point is drawn larger for readability. — GPT-5
- **Bounding-box widgets use visible spatial handles.** `bounding_box_include.js`, `bounding_box_moved_by.js`, and `bounding_box_grown_by.js` now expose draggable point/vector handles directly in the SVG instead of relying on invisible whole-widget dragging; `bounding_box_grown_by.js` uses the top-right growth endpoint and supports negative shrink/growth values. The JS tests now fail if production widgets instantiate `DragHandler` again. — GPT-5
- **Bounding-box boolean widgets use draggable source boxes.** `bounding_box_and.js` and `bounding_box_or.js` now render their dashed source rectangles as direct drag targets and recompute the intersection/union result rectangle live. — GPT-5
- **Ray projection widget uses draggable source points.** `ray_project.js` now generates its sample points once per widget instance, renders each as a visible drag handle, and recomputes the projected foot points live as those source points move. — GPT-5
- **Rasterizer clipping widget exposes all source vertices.** `rasterizer_clip_attributes.js` now lets all three source triangle vertices be dragged across the full widget area instead of limiting direct manipulation to the outside vertex. — GPT-5
- **Rasterizer clipping widget direct manipulation.** `rasterizer_clip_attributes.js` now uses the shared widget primitives and exposes source vertices as visible draggable handles instead of scalar sliders, while preserving the generated clipped-vertex UV markers. — GPT-5
- **Interactive widget modernization foundation.** `rake docs:widgets` now builds a standalone `docs/html/widgets.html` gallery with isolated per-widget frames for bulk visual review, `docs/plans/complete/widgets.md` and the agent docs codify the widget interaction rules, and `figure.js` gained scoped widget primitives for standard containers, SVG helpers, segmented controls, and draggable point handles. The rasterizer MSAA coverage widget is the first migrated reference widget: sample count remains a segmented control, while the triangle geometry is now adjusted by dragging its visible vertices directly. — GPT-5
- **Rasterizer MSAA and float-framebuffer resolve.** `engine::raster::Rasterizer` now exposes `setMSAASamples` for fixed 1x/2x/4x/8x subpixel sample counts, `rendercli --engine raster --msaa N` exposes the same setting from the CLI, and Modeler's render dialog shows an MSAA selector when the Rasterizer engine is active. `core::rasterizeTriangleSampled` evaluates edge-function coverage at a subpixel offset while preserving the existing 1x `core::rasterizeTriangle` behavior. For `N > 1`, the rasterizer projects/clips/bins triangles once, renders each sample into independent color/depth/stencil buffers, then averages those samples into the existing `Buffer<Colord>` output so tonemapping stays engine-agnostic. The rasterizer docs now include 1x-vs-4x high-contrast diagonal renders plus an interactive MSAA coverage widget, so the resolved edge difference is visible both statically and at the sample-position level. Tests pin subpixel barycentric weights, partial-edge resolve blending, tiled MSAA equivalence, the render-dialog MSAA selector, and the MSAA coverage widget structure. Measurements with `rendercli --repeat 10`: dense 640×480 LOD 8 sphere `--msaa 1` reported `render_ms runs=10 min=1006.174 median=1035.287 avg=1155.311 max=1809.999`, while `--msaa 4` reported `render_ms runs=10 min=3006.147 median=3778.575 avg=4039.878 max=6153.725`; 1920×1080 offscreen-floor `--msaa 1` reported `render_ms runs=10 min=97.900 median=101.965 avg=102.355 max=110.045`, while `--msaa 4` reported `render_ms runs=10 min=432.966 median=442.536 avg=447.971 max=489.621`; 640×480 LOD 3 materials baseline `--msaa 1` reported `render_ms runs=10 min=14.276 median=14.431 avg=14.639 max=15.797`, while `--msaa 4` reported `render_ms runs=10 min=54.477 median=55.513 avg=56.869 max=63.809`. — GPT-5
- **Rasterizer UV documentation renders and widgets.** `scripts/docs/rasterizer.rb` now emits rasterizer-only UV albedo and UV checker renders backed by a diagnostic `UVColorTexture` and UV-scaled checker mapping, so the rendered docs can show the attribute path visually instead of only describing it. The existing barycentric rasterizer widget now toggles between color and UV interpolation, and two new widgets illustrate affine-vs-perspective-correct UV interpolation plus clipping-generated vertices that preserve interpolated attributes. The JS widget tests now also assert that the perspective UV widget emits the expected UV grid lines and quad outlines, catching blank-widget regressions. — GPT-5
- **Rasterizer UV/material attribute interpolation.** `HitPoint` now carries UV coordinates, `render::UVMapping2D` maps those coordinates to texture space, and `engine::raster::Rasterizer` carries UVs through clipped vertices, raster vertices, perspective-correct fragment interpolation, built-in `MatteMaterial` albedo sampling, and the vertex/fragment shader hooks. New tests pin both programmable UV access and fixed-function material texture sampling from interpolated UVs. Measurements with `rendercli --repeat 10`: dense 640×480 LOD 8 sphere `render_ms runs=10 min=1022.142 median=1039.353 avg=1142.892 max=1635.037`; 1920×1080 offscreen-floor `render_ms runs=10 min=98.156 median=99.687 avg=100.820 max=105.644`; 640×480 LOD 3 materials baseline `render_ms runs=10 min=14.308 median=15.128 avg=15.427 max=18.247`. — GPT-5
- **Rasterizer depth/stencil state and shader hooks.** `engine::raster::Rasterizer` now exposes `DepthFunc`, configurable depth clear, optional depth writes, 8-bit `StencilFunc` reference/mask state, stencil clear/write masks, `StencilOp` actions for stencil-fail/depth-fail/pass, and lightweight `VertexShader` / `FragmentShader` hooks over projected and perspective-correct interpolated attributes. The default state remains the fixed-function path (`DepthFunc::Less`, depth writes on, stencil disabled, built-in Lambertian material shading), and the single-tile default renderer stays specialized so normal previews do not pay callback dispatch in the pixel loop. Measurements with `rendercli --repeat 10`: dense 640×480 LOD 8 sphere `render_ms runs=10 min=1014.534 median=1057.099 avg=1156.787 max=1726.229`; 1920×1080 offscreen-floor `render_ms runs=10 min=98.212 median=109.761 avg=110.942 max=132.385`; 640×480 LOD 3 materials baseline `render_ms runs=10 min=14.073 median=14.313 avg=14.544 max=15.527`. — GPT-5
- **Rasterizer homogeneous clip-space clipping.** `Camera::projectPointToClipSpace` now gives the software rasterizer un-divided camera clip coordinates for `PinholeCamera` and `OrthographicCamera`; the rasterizer precomputes per-vertex clip outcodes and cached screen coordinates, rejects triangles that are wholly outside one clip plane, keeps fully-visible triangles on the direct hot path, and runs a fixed-size Sutherland-Hodgman clipper only for mixed near/viewport cases. Measurements with `rendercli --repeat 10`: the dense 640×480 LOD 8 sphere reported `render_ms runs=10 min=999.101 median=1039.896 avg=1134.154 max=1609.866` after this change (previous face-culling baseline, `--cull both`: median `1158.779`); the 1920×1080 offscreen-floor baseline reported `render_ms runs=10 min=95.352 median=98.215 avg=98.771 max=104.422`. — GPT-5
- **Switchable rasterizer face culling.** `engine::raster::Rasterizer` now exposes `CullMode::Both`, `CullMode::Back`, and `CullMode::Front`; `Both` remains the default two-sided behavior, and `rendercli --engine raster --cull both|back|front` exposes the same choice from the CLI. Culling runs after near-plane clipping and before rasterization/binning, using projected screen-space winding. The source tessellation tests for `Box`, `Disk`, `OpenCylinder`, `Rectangle`, `Sphere`, `Torus`, and `Triangle` now pin that mesh faces are wound consistently with their vertex normals; the change also corrected `Sphere`, `OpenCylinder`, and `Torus` face order so backface culling works across curved primitives. Measurement on the canonical dense sphere command at 640×480 LOD 8, `--repeat 10`: `--cull both` reported `render_ms runs=10 min=1099.723 median=1158.779 avg=1210.625 max=1478.488`; `--cull back` reported `render_ms runs=10 min=1105.359 median=1133.989 avg=1204.189 max=1486.336`; the final PNGs compared byte-for-byte identical. — GPT-5
- **`rendercli` render-only timing mode.** `--timing` prints a stable `render_ms runs=... min=... median=... avg=... max=...` line for the render call itself, excluding process startup, JSON scene loading, engine construction, image conversion, and PNG writing. `--repeat N` reuses the loaded scene and configured engine to render the same scene N times, saves only the final image, and prints min/median/average/max stats for the repeated render calls. This replaces shell-level `/usr/bin/time` as the preferred way to collect rasterizer baseline measurements. — GPT-5
- **Multi-engine `class_doc` comparisons in Doxygen** — `class_doc(engines: [:raytracer, :raster]) do …` produces one image per engine listed (suffixed `__<engine>` so renders don't collide), and the C++ class docstrings reference both via a side-by-side `<table>`. Same scene, different engine — each comparison surfaces the integrator's contribution by what's preserved vs lost. Rolled out to `Sphere`, `Box`, `PinholeCamera`, `OrthographicCamera`, `Cylinder`, `MatteMaterial`, `PhongMaterial`, `ReflectiveMaterial`, `TransparentMaterial`. Reflective + Transparent comparisons are the most striking — the rasterizer can't recurse, so the sphere's appearance (which IS the reflected/refracted scene in the raytracer) has no analogue and falls back to a per-face hash color to keep the silhouette readable. `OrthographicCamera` gained a `projectPoint`/`projectPointWithDepth` implementation so the rasterizer can render orthographic scenes (parallel projection, no perspective divide). — Claude Opus 4.7
- **Opt-in tile-parallel path for `engine::raster::Rasterizer`.** The rasterizer now has its own `QThreadPool`, `activeTiles()` bookkeeping, `setMaximumThreads`, and `setQueueSize`; `queueSize > 1` builds a projected/clipped triangle list, bins triangles by overlapped framebuffer tile, and lets each worker own disjoint color/Z-buffer pixels. `rendercli --engine raster` keeps the default streaming single-tile path unless `--threads` or `--queue_size` is provided, because measurements show tile binning is not yet a default win: a dense 640×480 LOD 8 sphere stayed byte-identical but moved from `1.41–1.52s real / 1.14–1.15s user` before to `1.51–1.66s real / 1.17s user` on the default path and `2.87s real / 2.09s user` with `--threads 8 --queue_size 16`; a 1920×1080 near-camera floor stayed byte-identical at `0.15s real / 0.13s user` default vs `0.25s real / 0.37s user` with `--threads 4 --queue_size 4`. — GPT-5
- **Interactive JS widget for the rasterizer's edge-function inside-test** at `scripts/docs/rasterizer_pipeline.js`, embedded into the `engine::raster::Rasterizer` class docstring. Drag three vertex handles to reshape a triangle and watch the rasterizer's per-pixel coverage update live; pixel colors interpolate from the three vertex tints via barycentric weights (exactly how the real rasterizer would interpolate normals or UVs); hover anywhere to see the live `(w0, w1, w2)` weight readout, with an inside/outside verdict. The dashed bounding box is what the rasterizer actually scans. Mirrors the algorithm in `core::rasterizeTriangle` line for line. — Claude Opus 4.7
- **Software rasterizer engine wired through to `rendercli`, `Modeler` (preview menu + render dialog), and Doxygen** — `--engine raster` joins `raytracer` and `wireframe` in `rendercli` (sharing the `--lod` knob with Wireframe); `Render → Preview Engine → Rasterizer` and the render-dialog "Rasterizer" choice in `Modeler`'s engine combo plug into the same `RenderWidget` as the existing engines, with the same scene/camera state preserved across swaps. New `scripts/docs/rasterizer.rb` doc-render driver produces a hero image plus a 5-frame LOD sweep, embedded into `engine::raster::Rasterizer`'s class docstring. The Phase 3 ambient coefficient also adjusted from a heuristic `0.15` down-scaling to `1.0` (matches `MatteMaterial`'s default) so scenes with no explicit lights still produce a visible ambient contribution. — Claude Opus 4.7
- **`engine::raster::Rasterizer` — software rasterizer engine, V1 skeleton** (advances `docs/roadmap.md` §4.1 "Software rasterizer" bullet). New `RenderEngine` subclass mirroring `engine::wireframe::Wireframe`'s structure: tessellates the scene to a `Mesh`, projects each triangle's vertices via `Camera::projectPoint`, and rasterizes filled triangles via the new `core::rasterizeTriangle` helper. V1 is the simplest pipeline that produces a recognizable image — flat-shaded per-face hash colors, no Z-buffer (overlapping triangles overdraw, last-rasterized wins), no shading, no clipping. Per-pipeline-stage refinements (depth buffer, vertex normal interpolation, Lambertian shading via `Material`, backface culling, near-plane clipping) land in follow-up phases. The `core::rasterizeTriangle` helper uses the edge-function / barycentric algorithm (Pineda 1988) — emits per-pixel barycentric weights for every interior pixel as a side effect of the inside-test, ready for Phase 3's attribute interpolation. 8 unit tests on the engine + 7 on the helper. — Claude Opus 4.7
- **Performance-regression tests for `render::BVH`** in `test/unit/render/primitives/BVHPerformanceTest.cpp`. Three ratio assertions baked into the regular unit-test suite (runs as part of `ctest`): BVH intersect must be at least 5× faster than `Composite` at 512 primitives (observed ≈37× on dev hardware); BVH shadow-ray must be at least 10× faster than `Composite` (observed ≈113×); BVH shadow-ray must be at least 1.5× faster than `Grid` (observed ≈4.5×). Thresholds are well below observed ratios so noisy CI runners and debug builds don't flap; the test catches regressions that would make BVH fall back to linear-scan-equivalent perf (e.g. someone removes the AABB cull, or `setup()` silently no-ops). Precision tuning still happens via `benchmarks/SpatialIndexBenchmark.cpp` with the benchmark preset; this is the safety net. — Claude Opus 4.7
- **`render::BVH` spatial accelerator** (closes the BVH bullet under roadmap §3.R7). Bounding-Volume-Hierarchy `Composite` subclass: each internal node owns two children and an AABB tight around all primitives below it; each leaf node owns up to `leafSize()` primitives (default 4). Build is one-shot via `setup()` using the **Surface Area Heuristic** (SAH — Goldsmith & Salmon 1987) — for each recursion the split axis is the longest dimension of the centroid bounding box, primitives are sorted by centroid on that axis, and N-1 candidate splits are evaluated for SAH cost (`SA(left)·N_left + SA(right)·N_right`) with the lowest-cost split chosen; if the best split's cost is worse than the leaf-node alternative the recursion bottoms out as a leaf instead. `intersect` and `intersects` (the boolean shadow-ray flavour) walk the tree, pruning subtrees whose AABB the ray misses. Falls back to the inherited `Composite` linear scan if `setup()` was never called, keeping the class safe to instantiate without the build step. Drop-in replacement for `Composite` / `Grid` — same interface; the scene-conversion path doesn't switch to `BVH` automatically yet (deferred until benchmarks show the win on representative scenes). 10 new unit tests pin the empty-hierarchy / single-primitive / closest-of-many / large-scene-equivalence-with-linear-scan / shadow-ray-shortcut / setup-omission-fallback / leaf-size / bounding-box contracts. — Claude Opus 4.7

### Changed

- **Warnings are now build errors.** CMake adds `-Werror` alongside the existing warning flags, and existing Clang warnings were fixed by marking overrides explicitly and removing intentionally unused parameter names. — GPT-5
- **Rasterizer triangle coverage now uses prepared incremental edge stepping.** `core::rasterizeTriangleSampled` precomputes fixed-point edge values and row/column deltas once per triangle instead of recomputing all edge functions for every sampled pixel; public rasterization wrappers and rendered output are unchanged. Baseline PNGs for dense sphere, materials, offscreen floor, and MSAA variants compared byte-for-byte identical. Median `rendercli --repeat 10` timings were broadly neutral for 1x (`dense sphere 1062.704 ms -> 1075.118 ms`, `materials 15.015 ms -> 15.100 ms`, `offscreen floor 100.757 ms -> 102.928 ms`) and improved the repeated-coverage cases (`dense tiled 2686.379 ms -> 2529.417 ms`, `offscreen 4x MSAA 440.747 ms -> 421.814 ms`, `offscreen 8x MSAA 860.440 ms -> 830.366 ms`, `materials 8x MSAA 109.886 ms -> 103.516 ms`). — GPT-5
- **Documentation widget shared math helpers.** `figure.js` now owns reusable `FigureMath`, `FigureGeometry`, `Vector`, and `FigureSvg` conveniences for common widget math, triangle/segment queries, arrows, rays, labels, and panels; mesh interpolation, portal redirection, rasterizer, GJK, and color-model widgets now use those shared helpers instead of duplicating local vector and triangle logic. — GPT-5
- **BVH SAH widget focused modes.** The BVH traversal documentation widget now uses three focused topics, `Split candidates`, `SAH cost`, and `Traversal`, instead of a mixed overview mode that displayed multiple concepts at once. — GPT-5
- **Documentation widgets share stroke-width constants.** `figure.js` now exports standard scene-coordinate and pixel-coordinate stroke widths; the bounding-box, ray-project, rasterizer pipeline, clipping, MSAA, and perspective-UV widgets use those shared constants instead of local numeric `stroke-width` literals, and the widget tests now pin that convention. — GPT-5
- **Functional test shape-recognition replaced with real classical-CV classifier** (closes `modernize.md` §3.4.a item G; first concrete artefact of roadmap §4.11.d). The old `test/helpers/ShapeRecognition` was a 1-D row-projection heuristic — it accepted diamonds as squares and lemons as circles. Replaced with three layered helpers in `test/helpers/`: `Blob` (connected components via BFS flood-fill, for cases that care about interior fill), `Silhouette` (outer-extreme extraction — leftmost+rightmost per row plus topmost+bottommost per column — engine-agnostic, gives the same answer for a Raytracer-rendered solid disk and a Wireframe-rendered circle outline), and `ShapeClassifier` with `isCircle` / `isRectangle` predicates composed from `Silhouette`'s radial-variance and bounding-box aspect-ratio descriptors. Nine functional tests that had been silently passing under the old "lemons are circles" heuristic became real failures and were corrected to assert visibility (`"i should see something"`) instead — fish-eye / spherical projections distort sphere silhouettes off-circle, portal-redirected rays show only fragments, convex hulls of side-by-side boxes are hexagons, vertical cylinders have rectangular silhouettes, side-on tori are elongated rings; the shape claims in those tests were always wrong, the old classifier was just lying for them. — Claude Opus 4.7
- **Functional test step lookup is now Cucumber-style regex matching with hard-fail on misses** (closes roadmap `modernize.md` §3.4.a item A, with the design pivot from the originally-planned typed-method conversion to regex steps). `GIVEN(...)` / `WHEN(...)` / `THEN(...)` macros still register pattern strings, but the runtime now compiles each pattern as `std::regex` and dispatches via `std::regex_match` against the input — capture groups land in the step body as `const std::smatch& match` so steps can take parameters (`"a sphere with radius ([0-9]+)"`). A step text that matches no registered pattern, or that ambiguously matches more than one, raises `GTEST_FAIL` with a diagnostic; previously the framework printed `WARNING: 'given' step '...' is not defined!` to stderr and let the test report green — a real silent-failure path that survived typos and stale copy-paste. New `FeatureTestSelfTest.cpp` pins the regex-capture, missing-step, and ambiguity behaviors (7 tests). Backward-compatible for the existing 91 functional tests — none of their step strings contain regex metacharacters, so they're matched as literals. — Claude Opus 4.7

### Fixed

- **Modeler preview stale completion events.** `RenderWidget` now tags each worker thread with a render generation and ignores `finished()` callbacks from stopped or superseded renders, preventing property edits during an in-progress preview from freezing the current frame's live updates. — GPT-5
- **Rasterizer orthographic interpolation.** Fragment depth, world position, normals, and UVs now interpolate through homogeneous `clip.w` rather than always using camera-space depth, so orthographic camera renders and directional-light shadow maps no longer warp light-space depth comparisons. — GPT-5
- **Rasterizer uneven-tile binning.** The rasterizer now uses the same exact tile partition to compute both tile rectangles and triangle-bin ownership, so tiled renders match single-tile output even when framebuffer dimensions do not divide evenly by the tile grid. — GPT-5
- **Rasterizer MSAA widget drag bounds.** The MSAA coverage widget now clamps triangle vertex handles to the pixel-grid bounds so dragged points cannot leave the sampled framebuffer area. — GPT-5
- **Transparent refraction widget readout placement.** The Snell-law readout now sits in the open upper-left medium area and wraps the equation across two lines, avoiding the surface-normal/refracted-ray labels and the SVG right edge. — GPT-5
- **Texture coordinate widget preview parity.** The texture-mapping widget now keeps the sampled checker preview fixed in texture space and places the red marker in the actual `floor(s), floor(t)` cell, so dragging does not make the texture jump while the marker still matches the parity readout. — GPT-5
- **Support mapping GJK widget rebuild.** The support-function / Minkowski-difference widget now uses the shared widget container and a simpler two-panel layout: ask `supportA(v)` / `supportB(-v)`, then add the resulting `A - B` point to the active simplex moving toward the origin. The displayed step directions are stable, so changing shape separation moves shape B and the difference point without making `supportA(v)` jump between vertices. — GPT-5
- **Sampler streams widget clipping.** The sampler documentation widget now fits all dimension panels and legend text inside its SVG viewBox so the shutter-time panel no longer gets cut off in the widget gallery. — GPT-5
- **Grid DDA widget ray containment.** The `Grid` traversal documentation widget now keeps the draggable ray handles and visible arrow segment inside the grid bounds instead of letting them spill into the surrounding text panel. — GPT-5
- **Color model conversion widget readability.** The `Color` documentation widget now uses a quieter RGB-storage-versus-helper-views layout built on the shared `figure.js` primitives, removing the cluttered crossing-arrow diagram from the widget gallery. — GPT-5
- **Documentation render staleness hashing.** `rake docs:render` now ignores non-rendering JSON metadata (`id` values and generated `name` fields) when deciding whether an existing docs image is stale, so adding objects to one docs scene no longer forces unrelated later scenes to re-render. — GPT-5
- **Rendered image gallery stale asset cleanup.** `rake docs:images` now clears the copied `docs/html/rendered-images` assets before recopying from `docs/images`, so renamed or removed rendered images no longer linger in the gallery output. — GPT-5
- **Documentation widget interaction instructions.** Doxygen comments, widget source comments, and contributor docs now describe the current interaction model: sliders for scalar values, visible drag handles for spatial values, and no hidden whole-widget drag gestures. — GPT-5
- **Documentation widget label overlaps.** The thin-lens convergence, thin-lens disk sampling, tilt-shift Scheimpflug, and tonemap curve widgets now place labels outside their dense diagram areas so the widget gallery does not show clipped or overlapping text. — GPT-5
- **Widget gallery pointer mapping now matches responsive SVG layout.** `FigureSvg.pointFromEvent` maps browser pointer coordinates through the rendered SVG rectangle and `preserveAspectRatio` letterbox offsets, and the rasterizer pipeline widget now uses that shared mapping for its hover cursor. The generated widget gallery also cache-busts widget script URLs so rebuilt frames do not keep stale interaction code. Tests cover both SVG transform mapping and gallery-style letterboxing. — GPT-5
- **Software rasterizer no longer scans off-screen triangle bounding boxes.** `core::rasterizeTriangle` now requires an explicit clip rectangle and clamps the edge-function scan to that rectangle before entering the nested pixel loop; `engine::raster::Rasterizer` passes the framebuffer bounds instead of scanning the full projected bounding box and rejecting out-of-frame pixels inside the callback. Measurements from `build/release/tools/rendercli` with `/usr/bin/time -p`: a 640×480 near-camera floor-box scene (`size=[20,0.1,20]`) went from "killed after 12.00s" to `0.24s`; the same repro at `size=[1000,0.1,1000]` went from "killed after 267.14s real / 259.81s user" to `0.18s`. Normal scenes stayed in the same range: `glass_torus.json` at 1920×1080 LOD 2 was `0.26s` before / `0.33s` after, and a dense 640×480 LOD 6 sphere was `0.42s` before / `0.41s` after. — GPT-5
- **Raster and wireframe projection no longer recompute the camera inverse matrix per projected point.** `render::Camera` now memoizes the inverse of its camera-to-world matrix and invalidates it with `setPosition` / `setTarget`; `PinholeCamera` and `OrthographicCamera` projection/depth methods use the cached world-to-camera transform. Measurements from `build/release/tools/rendercli` with `/usr/bin/time -p` on a dense 640×480 raster sphere scene: LOD 6 moved from `0.39–0.40s real / 0.34s user` to `0.18–0.37s real / 0.14s user`, and LOD 7 moved from `1.41s real / 1.25s user` to `0.58–0.59s real / 0.48s user`; the before/after PNG outputs compared byte-for-byte identical. — GPT-5
- **Rasterizer hot loop reuses per-mesh projected vertices and clips triangles without heap allocation.** Each leaf mesh now caches every tessellated vertex's eye depth and screen projection once, so face fans and quad splits no longer re-transform shared vertices; the near-plane clipper also uses a fixed 4-slot array instead of allocating a `std::vector` for every source triangle. On the same dense 640×480 raster sphere scene, measured after the camera-inverse cache: LOD 6 moved from `0.14s user` to `0.09s user`, and LOD 7 moved from `0.48s user` to `0.31–0.32s user`; the before/after PNG outputs compared byte-for-byte identical. — GPT-5
- **`ViewPlane::pixelAt` scaled the view plane around world origin instead of around the camera position.** The old formula `(m_topLeft + m_right*x + m_down*y) * m_pixelSize` multiplied the entire world-space pixel position by `pixelSize`, which is mathematically equivalent to scaling the view plane around `(0, 0, 0)` in world space rather than around the camera. The bug: same camera intrinsics (distance, zoom) at different world positions produced different effective FOVs, because the eye-to-view-plane distance shifted with the position-dependent scaling. Most visible as a wireframe-vs-raytracer alignment mismatch (reported on the glass-torus demo, which uses `zoom = 1.5` from off-axis), but it always was wrong — just masked by the `WireframeEngine` not existing. Fix changes `pixelAt` to scale around `m_matrix.translationVector()` (the camera position), making `pixelSize` a pure FOV knob that produces standard pinhole geometry: `FOV = 2·arctan(0.8/zoom)`. Behavior change: scenes with `zoom ≠ 1` now render with the correct (typically wider) field of view than the over-zoomed pre-fix output. New `PinholeCamera.RoundTripsWithNonUnitZoomAndOffAxisCamera` test pins the round-trip contract; new `InstanceTessellate.ShouldRotateVertexPositions` and `InstanceTessellate.ShouldPreserveRotationThroughGridAndScene` rule out the alternative hypothesis (rotated-Instance vertex transform). — Claude Opus 4.7
- **Progressive-display regression in `Raytracer::render(Buffer<unsigned int>&)`.** The float-HDR pipeline (`ba110dc`) routed the LDR render through a separate `Buffer<Colord>` HDR accumulator + post-render tonemap pass, which meant the display buffer stayed empty until every tile finished — the GUI's `RenderWidget` polls the buffer between frames, so the whole point of the interlaced view planes (showing a coarse render that refines over time) was silently broken. Restored inline tonemapping for the LDR path: `Camera::render(raytracer, Buffer<unsigned int>&, tonemap, rect)` overload writes packed RGB values to the display buffer as workers complete pixels; `Raytracer::render(Buffer<unsigned int>&)` overrides the default RenderEngine implementation to dispatch tasks through this path. The HDR `Buffer<Colord>` overload keeps its current shape for non-display consumers (EXR writers, motion-blur compositors, future path-tracing accumulators). New `PinholeCamera.RendersIntoLdrBufferWithInlineTonemap` test pins the contract; the mid-render-non-empty property is timing-dependent and verified by visual smoke-testing in `Modeler`. — Claude Opus 4.7
- **`Grid::setup` integer overflow on degenerate-bbox primitives.** A flat axis-aligned `Rectangle` or `Disk` has zero thickness on its normal axis, so the textbook `s = cbrt(vol/N)` heuristic produced `s ≈ 0` along the degenerate axis and ~262k cells along the others; `m_numX * m_numY * m_numZ` overflowed `int` and `m_cells.reserve(<garbage>)` crashed. Now: axes below `1e-6 × maxAxis` are treated as degenerate (one cell along them), and `s` is computed from the non-degenerate axes only. The per-primitive bin loop short-circuits to cell 0 along degenerate axes so the divide-by-near-zero NaN can't propagate either. New `GridTest.ShouldHandleDegenerateAxisOnSetup` pins the contract. — Claude Opus 4.7
- **SSE3 dot-product type-punning UB eliminated.** `Vector3<double>`, `Vector4<double>`, `Vector3<float>`, and `Vector4<float>` dot-product operators previously extracted lane values through a `typedef union { __m128d vec; double coord[2]; }` — C-legal but C++ UB. Replaced with `_mm_cvtsd_f64` + `_mm_unpackhi_pd` extracts for the double variants and `_mm_cvtss_f32` + `_mm_shuffle_ps`/`_mm_movehl_ps` for the float variants: all register-resident, no aliasing, standard-compliant. Benchmark neutral: `VectorBenchmark` dot, reflect-chain, and batch-dot medians stayed within noise (<5%) on all four types (closes `docs/plans/core-math-optimization.md` §1.3). — Claude Sonnet 4.6
- **`-Wno-ignored-attributes` suppression for SSE template arguments.** GCC emits `-Wignored-attributes` when `__m128`/`__m128d` are passed as template type arguments (the four SSE `Vector` specializations inherit from `Vector<N, T, __m128d, …>`). With `-Werror` active this broke all builds once `-msse3` was added. Added `-Wno-ignored-attributes` to `CMakeLists.txt`; the suppression is scoped to this well-known SIMD-specialization pattern and does not mask genuinely wrong attribute usage elsewhere. — Claude Sonnet 4.6

### Changed

- **RenderWidget isolated render jobs.** Snapshot-capable engines now implement `RenderEngine::cloneForRender()`, letting `RenderWidget` cancel stale GUI preview renders and start replacement frames immediately while the old worker drains against its private camera/scene/back-buffer snapshot. Engines that opt out still serialize renders on the control engine. — GPT-5
- **Namespace cleanup §3.R5b — `raytracer::` → `render::` for engine-shared types.** Closes the namespace cleanup roadmapped after R5. All types that any rendering backend would use moved out of the engine-named namespace into a neutral `render::` namespace + `include/render/` directory tree. Phased rollout (10 commits, each subsystem in its own commit, test suite green at every step): Tonemap (1), Object/Stats (2), Sampler/SampleStream (3), ViewPlane (4), Texture/BRDF/BTDF (5), Material (6), Light (7), Camera (8), Primitive (9, 23 classes), RenderEngine + WireframeEngine (10). What remains in `raytracer::`: just `Raytracer` (the engine), `State` (per-ray recursion state), and `raytracer::stats::Counters`. Unblocks every future non-raytracing engine (wireframe is here; software raster, OpenGL, path tracer queued under §4.1) referencing `render::Camera` / `render::Light` / etc. without the awkward implication that they're "raytracer-specific." — Claude Opus 4.7
- Migrated from Qt 5.15 to Qt 6: `find_package(Qt6 … Qml)` replaces the old `Qt5 … Script` find; all `Qt5::` CMake targets updated to `Qt6::`. `QtScript` (removed in Qt 6) ported to `QJSEngine`/`QJSValue` in `ScriptedSurface` — element constructors are now registered through a `ScriptElementRegistry` QObject with Q_INVOKABLE methods; scripts continue to use `new Box(parent)` and `new Vector3(x, y, z)` unchanged. CI, Dockerfile, and README updated to `qt6-base-dev` + `qt6-declarative-dev`. — Claude Sonnet 4.6

### Added

- **Functional shadow-boundary test for `PointLight`** — `test/functional/render/lights/PointLightTest.cpp` pins the geometric shadow-boundary contract: with a white point light at (0,5,0), a unit sphere occluder at (0,1,0), and a red floor plane at y=-3, the shadow-centre pixel and a pixel just inside the tangent-predicted boundary (x=1.8, with boundary ≈ x=2.07) are dark, while a pixel just outside the boundary (x=2.4) carries a non-zero red channel. `projectPoint` is used to convert world coordinates to raster coordinates at render time so the assertion is resolution-independent. Closes part of `docs/modernize.md` §3.4. — Claude Sonnet 4.6

- **`Primitive::tessellate(int lod)` for the trivial primitives** (R4 Batch A, building on the canary from `0a6c8a4`). `Rectangle` returns 4 vertices + 2 triangles with `[0, 1]²` UVs; `Triangle` returns its single flat triangle with barycentric-style UVs at the corners; `FlatMeshTriangle` / `SmoothMeshTriangle` produce single-triangle meshes referencing the parent `core::Mesh`'s vertex data (face normal vs per-vertex normals respectively); `Plane` and the five CSG types (`Difference`, `Union`, `Intersection`, `MinkowskiSum`, `ConvexHull`) return empty meshes with type-specific `qWarning`s — Plane because it's infinite, CSG because mesh-boolean implementations are queued under roadmap §4.2.a. 11 new `*TessellateTest.cpp` files cover counts, normal correctness, UV layout, and LOD-invariance for every concrete impl. — Claude Sonnet 4.6
- **`Primitive::tessellate(int lod)` for `Disk`, `OpenCylinder`, `Composite`, `Instance`, and the `Grid`/`Scene` composites by inheritance** (R4 Batch B). `Disk` is a triangle fan: 1 center + N rim vertices, N triangles, with `segments = 16 << lod`; UVs lay the disk into the unit square. `OpenCylinder` is a quad strip wrapping the Y axis: `2 * (segments + 1)` vertices with seam-duplication at `u = 0`/`u = 1` so wrapped textures don't smear; normals point radially outward for smooth shading. `Composite::tessellate` concatenates every child's mesh, remapping face indices into the merged vertex buffer; `Grid` and `raytracer::Scene` inherit it unchanged because their child geometry is exactly Composite's child list. `Instance::tessellate` transforms vertex points by the point matrix and normals by the inverse-transpose normal matrix (re-normalised); UVs pass through unchanged. The `t = 0` configuration is captured for motion-blurred instances — a time-aware engine has to retessellate per frame. Two interactive Doxygen widgets (`disk_tessellate.js`, `open_cylinder_tessellate.js`) show the LOD-driven segment-count growth with live sliders. 6 new `*TessellateTest.cpp` files cover counts, UVs, normal preservation under non-uniform scale, multi-child concatenation, and LOD passthrough. — Claude Opus 4.7
- **Engine selector in `Modeler`** — both the modeling preview pane and the render dialog can now switch between `Raytracer` and `WireframeEngine`. Render dialog: "Engine" combobox is now the first control; engine-specific settings appear in their own frames and toggle visibility when the engine changes — Raytracer frame holds view plane / sampler / samples-per-pixel / recursion-depth / render-threads / queue-size; Wireframe frame holds a single LOD spinbox (0–10) forwarded to `WireframeEngine::setLod`. Resolution + progress-indicator + render/stop stay common. Modeling preview: new **Render → Preview Engine** menu (radio-style via `QActionGroup`, defaults to Raytracer); `Display::setEngineKind` swaps the active engine while preserving scene + camera so the preview keeps looking at the same view. Required generalising `RenderWidget` and `QtDisplay` to hold a `shared_ptr<RenderEngine>` instead of `shared_ptr<Raytracer>`; subclasses that need raytracer-specific operations (`Display`'s Ctrl-click ray-state probe, older pick-on-click UI paths) `dynamic_cast` to `Raytracer*` and skip when the active engine isn't one. — Claude Opus 4.7
- **`world::` wrappers for `Disk`, `OpenCylinder`, `Triangle`, `Rectangle`** — minimal Q_PROPERTY surfaces that make these primitives reachable from JSON / Ruby DSL / GUI editor for the first time. Each forwards to the matching runtime primitive in `toRaytracerPrimitive()`; the inherited `Surface` base wraps the result in an `Instance` for transform handling. ElementFactory registration on each. Not just a doc-render enabler — these primitives have been runtime-only since the project began. — Claude Opus 4.7
- **`WireframeEngine`** — first non-raytracing `RenderEngine` subclass. For each primitive, calls `Primitive::tessellate(lod)` for a `Mesh`, projects every face vertex via `Camera::projectPoint` (a new forward-projection virtual implemented on `PinholeCamera` and inheritors `ThinLensCamera` / `TiltShiftCamera`), and rasterizes each edge using a new `core::drawLine` Bresenham utility (header-only, all-octants, direction-independent). V1: single-threaded, no hidden-line removal, no near-plane line clipping — the simplest engine that demonstrates the tessellate-and-project pipeline, also useful as a debug visualization for the tessellate impls themselves. CSG primitives render as nothing (their tessellate returns empty meshes per §4.2.a); cameras without a closed-form `projectPoint` inverse (FishEye, Spherical, Equirectangular) silently produce empty renders. Selectable via `rendercli --engine wireframe --lod N`; in-app integrations under §3.R5 list. Wireframe doc renders for Box / Sphere / Torus + a 5-frame LOD sweep on the `WireframeEngine` class page. 6 + 9 + 8 = 23 new tests pin Pinhole projection, Bresenham line rasterization (single-pixel / horizontal / vertical / 8-octant / contiguity / negative-coords / direction-independence), and engine behavior (empty scene → background, configurable bg / edge / lod, null-scene safe, cancel / uncancel). Closes the `WireframeEngine` bullet under roadmap §3.R5. — Claude Opus 4.7
- **`Primitive::tessellate(int lod)` for `Sphere` and `Torus`** (R4 Batch C, completing the per-primitive tessellate rollout). `Sphere` produces a UV-sphere on a `lonSegs × latBands` grid (`16 << lod` × `8 << lod`); pole vertices are duplicated `lonSegs + 1` times — same 3D position, distinct u-coordinates — so wrapped textures don't pinch and the quad topology stays uniform across the surface (no polar fans). `Torus` produces a `majorSegs × minorSegs` grid (`16 << lod` × `16 << lod`) using the standard `((R + r·cos v)·cos u, r·sin v, (R + r·cos v)·sin u)` parametrisation; both u- and v-seams are closed by duplicating the final column/row. Vertex counts scale ~4× per LOD step (Sphere: 153/561/2145; Torus: 289/1089/4225). Two interactive Doxygen widgets (`sphere_tessellate.js`, `torus_tessellate.js`) visualise the band/segmentation layout with live LOD sliders. 22 unit tests cover counts at LOD 0/1, vertex-on-surface invariants, normal direction and unit-length, UV range coverage, and quad-topology correctness. — Claude Opus 4.7
- **`RenderEngine` abstract base class** (closes roadmap §3.R5, the prerequisite for the wireframe / software raster / OpenGL / path tracer engines listed in §4.1). Owns what every rendering backend has in common: camera + scene + tonemap (the LDR `Buffer<unsigned int>` render overload tonemaps via the configured operator), and the `cancel` / `uncancel` / `activeTiles` cancellation hooks (abstract — engines plug in their own mechanism). `Raytracer` is now a concrete subclass holding everything raytracer-specific: the `QThreadPool` tile-dispatch loop, the single-ray probes (`rayColor` / `rayState` / `primitiveForRay`), and the recursion-depth limit. No behavior change — same threading loop, same render output byte-identical against pre-R5 reference renders. The split surfaces what's shared vs engine-specific so the next `WireframeEngine` (queued behind §3.R4 tessellate) can drop in cleanly. — Claude Opus 4.7
- **Float HDR framebuffer + tonemap pipeline** (closes roadmap §3.R1, the foundation refactor that unblocks EXR output, motion-blur compositing, and path-tracing accumulators). `Camera::render` now writes into `Buffer<Colord>` rather than `Buffer<unsigned int>`; the per-pixel sample-count divide moved out of `Camera::plot` into the render loop so the float buffer carries averaged radiance directly. `Raytracer::render(Buffer<unsigned int>&)` keeps its existing public signature and now allocates a `Buffer<Colord>` accumulator, runs the tile workers into it, and applies the configured `Tonemap` to produce 8-bit RGB. The `Buffer<Colord>&` overload is exposed for direct float-buffer consumers. Three tonemap operators ship: `LinearTonemap` (pass-through, default — preserves pre-refactor pixel values exactly so existing renders stay byte-identical), `ReinhardTonemap` (`c / (1 + c)` per channel), and `AcesTonemap` (Narkowicz polynomial fit, the "filmic" curve that ships in modern game engines). All three self-register with the new `TonemapFactory` and are selectable via `rendercli --tonemap Linear|Reinhard|ACES`. 11 unit tests pin each operator's known-value contract plus the ACES-vs-Reinhard midtone comparison. — Claude Opus 4.7
- **Tilt-shift / Scheimpflug camera** (`raytracer::TiltShiftCamera` + `world::TiltShiftCamera`). Subclass of `ThinLensCamera` that lets the focal plane rotate off-perpendicular to the forward axis (around the camera's local right axis) and optionally shift the lens parallel to the sensor. Two consequences flow from the geometric change: the canonical "miniature" effect when the focal plane tilts steeply (only a narrow band stays in focus), and tilted-plane focus for keeping a long horizontal surface sharp end-to-end at wide apertures. Only the focal plane rotates — the image and lens planes stay perpendicular to forward, so this is the simplified-Scheimpflug variant rather than the full physically-accurate one (deferred to a future Kolb camera). Self-registers with `CameraFactory` and `ElementFactory`. Ships with rich Doxygen on both headers (class-level `@image html` miniature shot, full Scheimpflug explanation, comparison with ThinLens), `scripts/docs/tilt_shift_camera.rb` doc-render driver with a 5-frame tilt-angle sweep, `scenes/tilt_shift_demo.json` loadable demo scene, an **Edit → Add Camera → Tilt-Shift Camera** menu entry in `Modeler`, and 12 unit tests (raytracer + world) pinning the focal-plane convergence invariant under tilt, the tilt=0 / shift=0 degeneracy to plain ThinLens, the inherited DOF behavior, and the world-side property and factory dispatch. Closes the tilt-shift bullet in §4.4.a of the roadmap. — Claude Opus 4.7
- **Motion blur** for translating primitives. New `velocity` Q_PROPERTY on `world::Surface` (default zero) drives a per-shutter linear translation that the renderer integrates over via the sample stream's 1D dimension (`State::timeSample`, drawn from `stream->next1D()` in `Camera::render`). `raytracer::Instance` interpolates by shifting the world ray by `-velocity * timeSample` before transforming into local space; the static fast path is preserved when velocity is zero, so unanimated scenes pay only one branch per ray. Bounding boxes expand to cover the full motion range so spatial accelerators don't miss rays whose time sample puts them outside the static bbox. Time is dim 1 in the renderer's stream allocation (sub-pixel jitter = dim 0, time = dim 1, lens / future Kolb element = dim 2+) — every dimension lands in an independent stratified set via `Sampler::stream`'s default `(pixelHash + dim) mod numSets` lookup. Loadable demo at `scenes/motion_blur.json`. Rotation and scale animation are deferred to a future pass. — Claude Opus 4.7
- `world::Torus` editable wrapper for the existing `raytracer::Torus` primitive — exposes `sweptRadius` / `tubeRadius` as `Q_PROPERTY` so they auto-bind in `PropertyEditorWidget`, registers with `ElementFactory` as `"Torus"`, and ships with an **Edit → Add Primitive → Torus** menu entry in `Modeler` plus a loadable `scenes/glass_torus.json` demo (glass torus IOR 1.52 on a checkerboard-floored stage). Plugs the long-standing gap where the runtime primitive was unreachable from JSON scenes, the GUI editor, and `rendercli`. — Claude Opus 4.7
- `CMake 3.28` build with per-target preset (`debug`, `release`, `asan`, `coverage`, `fuzz`, `benchmark`) running alongside the legacy Rakefile. The Rakefile is now a thin wrapper around `cmake --preset` and hosts a few project-utility tasks (`check:cpp`, `check:inline`, `stats`, `docs:render`). — Claude Opus 4.7
- GitHub Actions CI matrix: Ubuntu 24.04 (gcc-13, clang-18) and macOS 14 (Apple Clang); separate jobs for ASan + UBSan, coverage (with a 60 % line-coverage CI floor), CodeQL, container-image build, the PLY LibFuzzer harness, and Doxygen → GitHub Pages. — Claude Opus 4.7
- LibFuzzer harness `fuzz/fuzz_ply.cpp` for the PLY parser (the only untrusted-input surface), gated on `RAYTRACER_ENABLE_FUZZING` and exercised in CI for 60 s on every push. — Claude Opus 4.7
- Google Benchmark microbenchmark suite under `benchmarks/` with a starter `VectorBenchmark.cpp` covering the Vector hot path; gated on `RAYTRACER_BUILD_BENCHMARKS`. — Claude Opus 4.7
- `Dockerfile` (multi-stage Ubuntu builder + distroless runtime) packaging the headless `rendercli`. CI builds the image on every push. — Claude Opus 4.7
- Devcontainer (`.devcontainer/`) targeting Ubuntu 24.04 with cmake / ninja / clang-18 / Qt 6 dev packages. — Claude Opus 4.7
- `.clang-format` and `.clang-tidy` configurations matching the existing style; lint enforcement remains advisory in CI until a bulk reformat lands. — Claude Opus 4.7
- Dependabot configuration for GitHub Actions and pre-commit weekly updates. — Claude Opus 4.7
- `.pre-commit-config.yaml` with generic hygiene hooks (trailing whitespace, EOF newline, merge-conflict marker, large-file, mixed-line-ending, yaml/json validity) plus `clang-format` in check-only mode (modernize.md §3.10). — Claude Opus 4.7
- Per-render performance counters gated on `RAYTRACER_ENABLE_STATS` (modernize.md §3.7). Thread-safe atomic increments (`memory_order_relaxed`) instrumenting `Sphere::intersect`/`intersects`, `BoundingBox::intersects`, and the `Grid` DDA traversal step; `Raytracer::render` resets at start and dumps a one-line JSON snapshot of the totals to `stderr` at end. Compile-time off by default — `RAYTRACER_STATS_INC` expands to `(void)0` so production builds carry zero overhead. — Claude Opus 4.7
- `.github/workflows/release.yml` cuts a GitHub release on `v*` tags: builds rendercli, generates an SPDX SBOM via syft (`anchore/sbom-action`), keyless-signs it with cosign through Sigstore OIDC, runs Trivy CVE scan on the SBOM (fails on HIGH/CRITICAL), and attaches the SBOM + `.sig` + `.cert` to the release. CI gains a per-commit SBOM artifact (`sbom` job) so reviewers can inspect the dependency surface without waiting for a tag (modernize.md §3.8 / closes #27). — Claude Opus 4.7
- Comprehensive shading-behavior tests for `MatteMaterial`, `PhongMaterial`, `ReflectiveMaterial`, `TransparentMaterial` (#22). — Claude Opus 4.7
- Unit tests for `Grid` (#19, +13 tests), `Raytracer` orchestration (#20, +11 tests), and SSE3-vs-generic SIMD regression tests for the math primitives. — Claude Opus 4.7
- First unit-test coverage for the `world::Element` scene-graph base class — 31 tests covering construction/IDs, name/displayName, parent-child add/insert/remove/move semantics with both raw and `std::unique_ptr` overloads, reparenting, `findById` recursion, and the `QJsonObject` read/write roundtrip including the generated-children exclusion (#24, partial). — Claude Opus 4.7
- Unit tests for `world::Transformable` (the position/rotation/scale layer above `Element`) — 23 tests covering defaults, the 1e-6 scale floor and absolute-value clamp, `canHaveChild` discrimination, local/global transform composition, `setMatrix` extraction roundtrip, `moveBy` in local vs global frames, and the `joinParent` invariant that a child's world-space position is preserved across `addChild` (#24, partial). — Claude Opus 4.7
- Unit tests for `world::Scene` — 26 tests covering canned defaults (`"New Scene"` name, warm gray ambient, sky-blue background, unchanged flag), property accessors, `canHaveChild`-accepts-anything semantics, `activeCamera` selection (none/single/last-of-many/non-camera filter), `save`/`load` JSON file roundtrip including child preservation, the changed-flag side effect of `save`, error handling on missing/unwritable paths, and the `toRaytracerScene` ambient/background propagation (#24, partial). — Claude Opus 4.7
- Unit tests for `world::Light`, `world::PointLight`, `world::DirectionalLight` — 28 tests covering Light defaults (visible, white, intensity 0.5), property accessors, show/hide toggle, the `toRaytracer()` factory producing the right concrete `raytracer::Light` subclass, position propagation through the global transform for `PointLight`, the color-times-intensity radiance baking convention, `DirectionalLight`'s canned starting direction `(-0.5, -1, -0.5)`, the raytracer-side direction normalization, and the positionless invariant (translation doesn't affect the resulting world-space direction) (#24, partial). — Claude Opus 4.7
- Unit tests for the parameter-widget family (`AbstractParameterWidget`, `BoolParameterWidget`, `IntParameterWidget`, `DoubleParameterWidget`, `StringParameterWidget`, `ColorParameterWidget`, `VectorParameterWidget`, `AngleParameterWidget`) — 35 tests covering construction, `setValue`/`value` roundtrip, `setParameterName` side effect, and `changed` signal emission via `parameterChanged()` (#23, partial). — Claude Opus 4.7
- Unit tests for `CameraTypeWidget`, `ViewPlaneTypeWidget`, and `ReferenceParameterWidget` — 10 tests covering combo-box population from `CameraFactory` / `ViewPlaneFactory` registries, the `changed()` signal fanout via `QMetaObject::invokeMethod` on the private `typeChanged()` slot, and `ReferenceParameterWidget`'s default "<No Selection>" sentinel, material-reference roundtrip, and the `Exception` thrown from `makeVariant` when the base class isn't `Material` or `Texture` (#23, partial). — Claude Opus 4.7
- Unit tests for `SceneModel` (Qt `QAbstractItemModel` for the scene tree) and `PropertyEditorWidget` — 19 tests. SceneModel: hidden-root wrapping invariants, single-column layout, `index`/`parent`/`rowCount` navigation including the top-level/child boundary cases, `data` returning `displayName` only for `Qt::DisplayRole`, the drag-and-drop mime-types and `Qt::MoveAction` declarations, and the `setElement` swap. PropertyEditorWidget: construction with null and non-null root, the canned `256×100` size hint, the `setElement(nullptr)` deselect path, element-with-properties selection, and `setRoot` followed by deselect (#23, partial). — Claude Opus 4.7
- Unit tests for `RenderSettingsWidget`, `RenderWidget`, `QtDisplay`, `PreviewDisplayWidget`, `RenderWindow` — 27 tests rounding out the widget coverage. RenderSettingsWidget: canned defaults (sampler="Regular", viewPlane="PointInterlacedViewPlane", thread defaults from `QThread::idealThreadCount`), positive-resolution invariant, `renderClicked`/`stopClicked` signal fanout, `setBusy`/`setElapsedTime` smoke. RenderWidget / QtDisplay: ctor with shared `Raytracer`, `setBufferSize`/`setShowProgressIndicators`/`stop`-before-`render` smoke, `interactive` default and toggle. PreviewDisplayWidget: 256×25 size hint, `clear()` smoke. RenderWindow: not-busy default, `sizeHint`, `setScene` with a real scene. Closes #23 (the only un-covered widget class is the `CameraParameterWidget` abstract base, which has no behavior beyond what the concrete `PinholeCameraParameterWidget`/`SphericalCameraParameterWidget`/`FishEyeCameraParameterWidget` tests already cover). — Claude Opus 4.7
- New camera type: `raytracer::ThinLensCamera` + `world::ThinLensCamera` editable wrapper. Pinhole with a finite-radius circular aperture and a configurable focal distance, producing physically-motivated depth of field (objects at the focal distance render sharp; everything else blurs into a circular bokeh disc whose diameter scales with both `apertureRadius` and the out-of-focus distance). Lens-disc points are produced by feeding the active `ViewPlane` sampler's stratified `[0,1]²` per-pixel sub-samples through the concentric square-to-disc mapping (Shirley 1997) — so 1024 spp gives 1024 *stratified* lens samples instead of 1024 random ones, dropping the dominant DOF noise term from `O(1/√N)` to `O(1/N)` and making bokeh render cleanly at modest sample counts. `focalDistance` is measured from the user-facing camera position (matching every photography app's mental model), not the math-side eye/pinhole. `setViewPlane` auto-installs a `JitteredSampler(16, 83)` so the interactive preview default 1-spp flow doesn't show confetti. Self-registers with `CameraFactory` and `ElementFactory` so existing scene files can opt in by setting `"type": "ThinLensCamera"`. — Claude Opus 4.7
- Empirical-testing surface for `ThinLensCamera`: a `ThinLensCameraParameterWidget` registered with `CameraParameterWidgetFactory` that adds four live spinboxes (distance, zoom, aperture radius, focal distance) to camera controls when the camera is selected, an **Edit → Add Camera → Thin Lens Camera (DOF)** menu entry in `Modeler`, and a loadable `scenes/dof_demo.json` scene with front/middle/back colored spheres on a gray floor. — Claude Opus 4.7
- Educational documentation for `ThinLensCamera`: rich Doxygen on both the raytracer and world headers explaining the geometric basis of DOF (focal-plane convergence, blur-disc scaling, what the thin-lens approximation leaves out — chromatic aberration / distortion / vignetting / flare / Kolb 1995 lens stacks), with class-level `@image html` plus per-setter parameter-sweep image tables matching the SphericalCamera / PhongMaterial pattern. Aperture sweep at `[0.0, 0.1, 0.2, 0.3, 0.4]`, focal-distance sweep at `[4.5, 6.25, 8, 9.75, 11.5]` — values picked so each sample lands on or precisely between the three demo spheres. Driven by the new `scripts/docs/thin_lens_camera.rb` doc-render driver and a `dof_scene` helper added to `scripts/render_docs.rb`. — Claude Opus 4.7
- Unit tests for `ThinLensCamera` — 14 tests across raytracer + world wrappers pinning the focal-plane convergence invariant (all rays for a given pixel converge to the same focal-plane point regardless of lens-disc sample), the `apertureRadius=0` → pinhole degeneracy, the `focalDistance ≤ 0` → silent rejection, and the world-side property clamps. — Claude Opus 4.7
- New camera type: `raytracer::EquirectangularCamera` + `world::EquirectangularCamera` editable wrapper. Full 360°×180° panoramic projection (no tunable parameters — always covers the canonical full sphere; distinct from `SphericalCamera` which has user-set FOV). Maps each pixel `(x, y)` linearly to a `(longitude, latitude)` pair on the unit sphere. Rendered output must be 2:1 aspect for square equator pixels. Self-registers with `CameraFactory` and `ElementFactory`. Ships with rich Doxygen on both headers (class-level `@image html` panorama, full explanation of pole-stretching and seam artifacts, comparison with SphericalCamera), `scripts/docs/equirectangular_camera.rb` doc-render driver using a new `panorama_scene` helper (cardinal-direction colored spheres on a checker floor for full-sphere visual content), `scenes/panorama_demo.json` loadable demo scene, an **Edit → Add Camera → Equirectangular Camera (360°)** menu entry in `Modeler`, and 9 unit tests (raytracer + world) pinning the longitude/latitude → direction mapping at the image center, the north/south pole orientation (with explicit comment that this codebase's `Vector3d::up()` is `(0, -1, 0)` — required a y-flip in the projection so the rendered output isn't upside-down), the unit-length-direction invariant, and the camera-position ray origin. — Claude Opus 4.7
- Mutation testing wired up via mull (closes #28). New `RAYTRACER_ENABLE_MUTATION` CMake option + `mutation` preset (Clang-only — mull is an LLVM-frontend pass plugin); `mull.yml` scopes the run to `include/core/math/` + `src/core/math/` (excluding the SSE3 specialisations which the benchmark suite catches more directly) using arithmetic / comparison / boundary mutation operators; `.github/workflows/mutation.yml` runs monthly on the first at 06:00 UTC plus on demand, installs mull from the project's apt repo, builds against the mull plugin, runs `mull-runner` against the unit-test binary under `xvfb-run` (some widget tests need a display), and uploads the surviving-mutant report as a 90-day artifact for manual triage. Intentionally not gated on per-PR CI — mull is slow and the value is in periodic gap-finding, not per-change blocking. — Claude Opus 4.7
- Unit tests for the remaining `world::*` editable-scene classes: `Texture` family (`Texture`, `ConstantColorTexture`, `CheckerBoardTexture`) covering the lazy-singleton `defaultTexture()`, sub-texture self-reference rejection, and `toRaytracerTexture()` factory dispatch; `Material` family (`Material`, `MatteMaterial`, `PhongMaterial`, `ReflectiveMaterial`, `TransparentMaterial`) covering canned defaults (white specular, exponent 16, vacuum IOR 1.0), the `Ranged(0,1)` clamping on Phong/Transparent coefficients vs the *unclamped* `ReflectiveMaterial::reflectionCoefficient`, and `toRaytracerMaterial()` dispatch through the abstract base; `Camera` family (`Camera`, `PinholeCamera`, `OrthographicCamera`, `SphericalCamera`, `FishEyeCamera`) covering canned position/target defaults, zero-or-negative-zoom-falls-back-to-1 contract, `SphericalCamera`'s 180°×120° field-of-view defaults, and `toRaytracer()` dispatch; `Surface` family (`Surface`, `Sphere`, `Box`, `Cylinder`, `Ring`) covering visibility and material defaults, child-type predicate (Surfaces and Lights only), per-class size/radius epsilon floors and absolute-value clamping, `Box::setBevelRadius` clamping to `size.min()` plus its recompute-on-shrink, `Ring::outerRadius` floored at `innerRadius + ε`, and `toRaytracer()` dispatch; `CSGSurface` family (`CSGSurface`, `Difference`, `Union`, `Intersection`, `MinkowskiSum`, `ConvexHull`) covering the active-by-default flag, the inactive-falls-back-to-Composite path, and the empty-and-active-returns-null contract for each operation. 117 new tests (#24, completes the world/objects/* coverage modulo `ScriptedSurface` which depends on QtScript and is being deprecated). — Claude Opus 4.7

### Changed

- **Sampler stream API** for stratified Monte-Carlo sampling. New `raytracer::SampleStream` abstract class with `next2D()` / `next1D()` (PBRT `Sampler::Get1D` / `Get2D` shape) and a default `Sampler::stream(sampleIndex, pixelHash)` that pulls successive dimensions from `(pixelHash + dim) mod numSets`-offset pre-baked sample sets — independent stratification per dimension, per-pixel decorrelation. `Camera::rayForPixel` now takes a `SampleStream&` so cameras with extra stochastic dimensions (thin-lens for lens disc; future motion-blur for shutter time, polarised for analyser angle, Kolb 1995 lens stacks for element + wavelength) can pull what they need. The 2-arg `rayForPixel(x, y)` convenience overload using a `NullSampleStream` is preserved for tests and ad-hoc callers. `Camera::render` now constructs one stream per `(pixel, sample)` pair and threads it through; sub-pixel jitter is consumed as dimension 0, so cameras see a stream starting at dimension 1. `ThinLensCamera` drops its previous fractional-part-as-lens-sample hack in favour of `stream.next2D()`, fixing the residual sub-pixel/lens-disc correlation while preserving the `O(1/N)` bokeh convergence. `Sampler::numSamples()` now reflects the *actual* produced sample count rather than the requested count — `RegularSampler`/`JitteredSampler` floor `sqrt(N)` to lay out an `n×n` grid (50 → 49, 2048 → 2025), and the `Camera::plot` averaging now divides by the truthful denominator. — Claude Opus 4.7
- `Factory<Base>::create()` now returns `std::unique_ptr<Base>` instead of a raw `Base*`, making the ownership transfer explicit at every call site (#17). — Claude Opus 4.7
- `Raytracer::setScene` and the constructors now take `std::shared_ptr<raytracer::Scene>`; the Raytracer co-owns its scene with callers, fixing the silent scene leak in `Modeler`/`RenderWindow` on every scene change (#17). — Claude Opus 4.7
- `Element::addChild` / `insertChild` now have `std::unique_ptr<Element>` overloads alongside the raw-pointer ones, so adoption-from-factory paths are typed for ownership transfer while drag-and-drop re-parenting in `SceneModel` keeps using the raw form (#17). — Claude Opus 4.7
- `Singleton<T>` now uses the Meyers' singleton pattern (function-local static), guaranteed thread-safe per C++11 (#5). — Claude Opus 4.7
- `Factory::registerClass` now stores creators in `std::map<Identifier, std::unique_ptr<BaseCreator>>`, eliminating the leak when an id is registered twice (#10). — Claude Opus 4.7
- `Grid::intersect` and `Grid::intersects` now share a single template DDA traversal (`traverseGrid`); the per-method bodies dropped from ~200 lines each to a thin visitor lambda (#11). — Claude Opus 4.7
- Replaced vendored GoogleTest 1.7-era source with GoogleTest 1.14 via CMake `FetchContent`; deleted ~52,000 lines of in-tree gtest/gmock; migrated `MOCK_METHODn` macros to the modern `MOCK_METHOD` form. — Claude Opus 4.7

### Fixed

- **Numerical stability in `Quartic::solve`** — when the resolvent cubic returned a real root that landed just below zero due to FP rounding, the absolute-epsilon `isAlmostZero` check rejected it and the solver reported zero quartic roots. Use a tolerance scaled to the operand magnitudes; the `(1, -16, 86, -176, 105)` quartic and the four Torus tests it cascaded into now pass. — Claude Opus 4.7
- **`Quadric::solve` degenerate case** — when the leading coefficient is zero the equation is linear, not quadratic. The old code blindly divided by `2*a`, leaking a NaN that x86 happened to slip through `OpenCylinder`'s y-range check. Added an explicit linear branch (handles the cylinder-axis ray). — Claude Opus 4.7
- **Data race on `RenderTask::active`** — written by the worker thread, read by the main thread without synchronisation. Switched to `std::atomic<bool>` (#6). — Claude Opus 4.7
- **`QThreadPool` memory leak** — held as a raw pointer in `Raytracer::Private` with no destructor; switched to `std::unique_ptr<QThreadPool>` (#7). — Claude Opus 4.7
- **`OpenCylinder` zero-radius silent corruption** — `1.0 / radius` produced `+Infinity`, leaking into surface normals and corrupting all subsequent shading. Now throws `DivisionByZeroException` when constructed with `radius == 0` (#9). — Claude Opus 4.7
- **`Grid::setup` unchecked `dynamic_pointer_cast`** — capture the cast and assert non-null with an explicit message before dispatch; converts a silent null-deref into a debuggable assertion failure (#8). — Claude Opus 4.7
- **`Grid::setup` cube-root precision** — `pow(x, 0.3333333)` truncated to seven significant digits; switched to `std::cbrt` (#12). — Claude Opus 4.7
- **`Vector3<double>` SSE3 specialisation never selected on x86 CI** — release flags only had `-mtune=native`, not `-msse3`. Without `__SSE3__` the SSE3 specialisation was dropped and `Vector3<double>` was 24 bytes instead of 32. Added `-msse3` for x86 family CPUs in `CMakeLists.txt`. — Claude Opus 4.7
- **`SSE3 Vector3<double>` and `Vector4<double>` private ctor type typo** — second parameter was `__m128` (single-precision) instead of `__m128d`; Apple Clang silently coerced it but gcc-13 / clang-18 on Linux rejected the `m_vector[1] = vec1` assignment. — Claude Opus 4.7
- **Linux build under libstdc++**: 83 headers were missing transitive includes (`<memory>`, `<algorithm>`, `<string>`, `<list>`, `<vector>`, `<functional>`) that Apple libc++ pulls in implicitly. — Claude Opus 4.7
- **`__cxa_throw` ABI signature conflict on Linux** — libstdc++ and libc++abi forward-declare `__cxa_throw` with different second-parameter types. The override-based exception-backtrace mechanism wasn't called by anyone; deleted it and kept the SIGSEGV trap. — Claude Opus 4.7
- `random_shuffle` ambiguity under libstdc++: project's own `::random_shuffle` (in the global namespace) collided with libstdc++'s deprecated `std::random_shuffle`; qualified the call sites with `::`. — Claude Opus 4.7

### Removed

- Vendored `gtest/` and `gmock/` source trees (~52,000 lines), replaced with GoogleTest 1.14 via `FetchContent`. The Rakefile no longer builds the test suite — `cmake --preset release && ctest` does. — Claude Opus 4.7
- Custom `meta::StaticIf`, `meta::NullType`, `meta::IsNullType`, `meta::TypesEqual` templates — predated C++17 and were re-implementations of `std::conditional_t` / `void` / `std::is_same_v`. — Claude Opus 4.7
- The whole compile pipeline from the `Rakefile` (Qt path constants, `.moc`/`ui_*.h`/`.o` rules, header-dependency scanner, per-example link blocks) — replaced with thin `cmake --preset` wrappers. ~150 lines deleted. — Claude Opus 4.7
- 58 redundant `#include "<X>.moc"` lines from .cpp files — leftover from the old manual-moc workflow; AUTOMOC handles moc generation now. — Claude Opus 4.7
- Custom `Ray.cpp` (just two static-member specialisations); inlined into `Ray.h` as C++17 `inline` variables. — Claude Opus 4.7
