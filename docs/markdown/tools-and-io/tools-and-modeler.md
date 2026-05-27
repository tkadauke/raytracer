# Tools and the Modeler

The library is the renderer. The checked-in front ends are the
small command-line renderer and the Qt modeling UI that turns scene
files into something visible and editable.

By the end of this chapter you should know:

- how `rendercli` loads and renders scene JSON,
- how the `Modeler` executable wires the shared render engines into a GUI,
- where reusable scene JSON files live.

## <a id="rendercli-the-headless-renderer"></a>`rendercli` — the headless renderer
[`tools/rendercli/`](../../../tools/rendercli/) is a command-line front end.
It reads a JSON scene file, builds the scene graph through the
[`world::`](../../../include/world/) wrapper layer, compiles a render graph from
the scene and command-line intent, runs the graph, and writes a PNG to disk.

The typical invocation:

```sh
$ rendercli --engine raytracer --width 1920 --height 1080 \
            scenes/dice.json \
            dice.png
```

The default path is graph-backed. The `--engine` flag sets the graph's preferred
executor (`raytracer` / `raster` / `wireframe`), while a scene-level
`renderIntent` can also choose the executor, structural view mode, and overlay
intent. `--direct_engine` bypasses the graph and renders with the selected
engine directly; that mode is useful for focused engine debugging and for
low-level knobs that are not yet represented as graph pass state. Rasterizer
controls such as MSAA, post-process AA, viewport/scissor state, and
color-output state are compiled into typed raster beauty pass state, while
preview shadow-map settings live on the graph shadow pass. Both are serialized
when graph JSON is exported.

The flags cover output size, sampler choice, samples-per-pixel, recursion
depth, tonemap operator, and per-engine knobs such as [LOD](../appendix/a-glossary.md#l),
[MSAA](../appendix/a-glossary.md#m), queue size, and thread count.

If the scene has a top-level `animation` block, `--frame N` evaluates the
world scene at frame `N` before the runtime render scene and active camera are
built:

```sh
$ rendercli --engine raster --frame 24 \
            scenes/animation_frame_demo.json \
            frame_0024.png
```

`--animation` renders a scene timeline as an image sequence. The output path
must contain one printf-style integer placeholder:

```sh
$ rendercli --engine raster --animation --frame_start 1 --frame_end 48 \
            scenes/animation_frame_demo.json \
            frames/frame_%04d.png
```

Grouped importer scenes can carry ordered step metadata on `Group` nodes.
`--step N` or `--step single:N` renders only that step's groups plus static
groups, `--step cumulative:N` renders every step through `N`, and
`--step sequence[:FIRST-LAST]` writes one cumulative build-view image per
available step. Sequence output paths use the same printf-style integer
placeholder rule as animation output:

```sh
$ rendercli --step sequence test/fixtures/rendercli/grouped_steps.json \
            steps/step_%02d.png
```

`rendercli` is the right front end for headless rendering, batch rendering,
documentation image generation, and timing runs.

It can also expose the compiled render graph without drawing pixels:

```sh
$ rendercli --render_graph_only --render_graph_format dot \
            scenes/dice.json \
            dice-graph.dot
```

The graph export formats are `text`, `dot`, and `json`. `--render_graph` is
accepted as an explicit spelling of the default graph-backed render path, and
`--render_graph_in plan.json` loads a saved JSON plan instead of compiling one.
If the scene JSON contains a top-level `renderIntent` block, rendercli uses
that as the graph compiler's base intent. Command-line graph options are
layered through the same `RenderGraphRequest` resolver that the Modeler preview
uses, so engine, view, AA, shadow, overlay, camera, shading, and AOV choices
share one interpretation before plan compilation.
General render controls that affect an underlying engine are also translated
into typed intent engine options before compilation. For example,
`--sampler`, `--samples_per_pixel`, `--depth`, `--threads`, and
`--queue_size` become raytracer pass state when the graph contains
`raytrace_beauty`; raster controls such as `--lod`, `--msaa`,
`--msaa_shading`, viewport/scissor, blending, alpha test, depth bias, and
shadow-map quality become raster pass or shadow-node state; wireframe `--lod`
becomes wireframe pass state. The compiler emits those parameters while
synthesizing nodes, so exported graph JSON is self-describing and replay does
not rely on hidden rendercli setup.
If that intent does not name a default camera, rendercli annotates compiled
scene-rendering passes with the active scene camera id.
Selector-specific scene intent is preserved by scene JSON, but graph
compilation currently rejects it until the compiler can synthesize real
scene-partitioning and composition passes.
When compiling a plan, `--render_graph_executor raytracer|rasterizer|wireframe`
overrides the graph intent's default executor, and
`--render_graph_view
default|beauty|wireframe|depth|stencil|stencil_composite|normal|object_id|material_id|world_position|raster_coverage_count|raster_depth_test_count|raster_depth_pass_count|raster_shade_count|raster_color_write_count`
overrides the graph intent's structural view mode. `--render_graph_camera
camera_id` overrides the intent's default scene-camera reference; current
executors still render with the active runtime camera, but the compiled graph
records the requested camera intent for inspection and future alternate-camera
execution. `--render_graph_shading_profile name` overrides the default named
shading profile, and repeated `--render_graph_shading_parameter key=value`
options attach scalar profile parameters. The compiler records that profile
intent on synthesized scene-rendering passes.
`--render_graph_view_override selector,key=value` adds one high-level view
override to the request. `all,executor=rasterizer,view=depth` is equivalent to
changing the default frame intent; selector-specific values such as
`tag:debug,view=wireframe` are accepted as render intent but currently fail
compilation until the graph compiler can split and composite selected scene
subsets. The
depth, stencil, normal, object-id, material-id, world-position, and raster
counter views
compile graph-visible AOV passes and visualization passes that write the final
color image. When the selected graph executor is the rasterizer, those AOV
passes use rasterizer diagnostic buffers, so they reflect tessellated raster
geometry and raster pass state rather than analytic primary-ray intersections.
The raster counter views are rasterizer-only heatmaps for coverage, depth tests,
depth passes, shaded fragments, and color writes, useful for spotting overdraw
or wasted shading in complex imported models. They use an absolute color scale:
black is zero work, cool colors are low counts, and red marks high repeated
work.
`--render_graph_aov_out depth=depth.png` writes an additional graph AOV preview
image while preserving the main render output; repeat the option for multiple
AOV files such as `stencil=mask.png`, `normal=normal.png`, or
`world_position=positions.png`.
When replaying explicit graph JSON, `--render_graph_color_in`,
`--render_graph_depth_in`, `--render_graph_stencil_in`,
`--render_graph_object_id_in`, and `--render_graph_material_id_in` bind imported
or history resources from image files with `resource=file` syntax before
execution.
`--render_graph_wireframe_overlay` asks the compiler to insert a graph-visible
wireframe overlay pass between the beauty pass and the tonemap pass.
For graph renders, `--post_aa fxaa` and `--post_aa smaa` are also
graph-visible: the shared compiler inserts a postprocess pass after the beauty
pass rather than hiding the filter inside one engine, and the pass's typed
`post_process_aa` parameters select the replayed filter. `--post_aa taa` stays
on the raster beauty pass until temporal history resources are graph resources.
Raster preview shadows compile as a `raster_preview_shadows` node feeding the
beauty pass. Its typed `shadows` parameters carry map size, cascades, bias, and
filtering. The shadow node builds the full directional/cascade collection
through the raster shadow-map builder, publishes a first-cascade depth preview
for inspection, and passes the full artifact to raster beauty. Disabling that
node leaves the raster beauty pass running without graph-controlled shadows.
Replayed graph JSON can also contain `composite/composite` passes tagged
`depth_composite` or `stencil_composite`; rendercli executes those with
graph-visible color, depth, and stencil resources.
Wireframe graph renders carry `--lod` in typed wireframe pass state, so
graph-only JSON exports and replayed graph renders preserve the requested
tessellation density. The Modeler raster render dialog uses the same intent
engine-option path for its raster quality controls before compiling the render
window graph.
`--disable_pass`, `--disable_pass_kind`, `--disable_executor`, and
`--disable_feature` apply graph overrides before validation or rendering.
Those controls are intentionally graph-level: disabling the required
`raytrace_beauty` pass, for example, makes validation fail before any image is
written. Replaying a saved graph uses the exported color resource dimensions,
so `--width` and `--height` only need to be supplied when they intentionally
match the saved plan.

`--render_graph_trace_out trace.json` writes the last executed graph trace as
JSON while rendering an image. The trace includes the executed plan, each pass's
status and elapsed time, the render-input fingerprint, supported input/output
resource preview metadata, cache status metadata, and available
difference-preview metadata. Trace capture is opt-in; ordinary graph renders
skip those diagnostic artifacts, while this flag enables them for the render
that is being exported. Graph-only mode cannot write a trace because no graph
execution happened.

That gives a two-step debugging loop:

```sh
$ rendercli --render_graph_only --render_graph_format json \
            scenes/dice.json \
            dice-graph.json
$ rendercli --render_graph_in dice-graph.json \
            scenes/dice.json \
            dice-from-graph.png
```

## <a id="src-modeler-the-interactive-editor"></a>`src/modeler` — the interactive editor
[`src/modeler/`](../../../src/modeler/) builds the `Modeler` executable. It is
the general scene modeling UI: a scene tree on the left, a property editor on
the right, a material/camera preview dock, a timeline dock when the scene has
animation, and a central live render preview.

Build and launch it from the release preset:

```sh
$ cmake --build --preset release --target Modeler
$ build/release/src/modeler/Modeler
```

The central `Display` widget is a subclass of
[`QtDisplay`](../../../src/widgets/QtDisplay.cpp), which is itself a subclass
of [`RenderWidget`](../../../src/widgets/RenderWidget.cpp). The inheritance
chain reflects the responsibilities:

- `RenderWidget` owns the framebuffer pair and render-job lifecycle.
- `QtDisplay` adds mouse-drag camera control.
- `src/modeler/Display.cpp` adds scene-camera timeline poses, preview intent
  selection, rasterizer-preview shadow toggling, and the Ctrl-click ray-state
  probe.

The editor can swap the live preview intent between Raytracer, Rasterizer, and
[Wireframe](../appendix/a-glossary.md#w). The preview itself is graph-backed:
the selected kind becomes the default executor in the compiled render graph,
while the scene and camera stay shared so the preview keeps looking at the same
thing across the swap. Property edits refresh preview geometry and materials
without resetting the live preview camera; opening a different scene still
starts from that scene's saved camera. The Elements dock exposes a generated
`Render Settings` item under the scene; selecting it opens the saved scene
intent in the property editor. Those properties write the scene's top-level
`renderIntent` block, not
normal child geometry. The editor groups the settings by role, uses dropdowns
for enumerated choices such as engine, view mode, sampler, and
postprocess AA, and only shows engine-specific fields for the selected default
engine. The same property editor has a search field for filtering long property
sets and collapsible groups so advanced scene/import settings can stay out of
the way. Internal execution controls such as view-plane type, worker thread
count, and queue size stay hidden in Modeler; Modeler's own preview/final
controls keep using the point-interlaced view plane and automatic execution
defaults, while lower-level values can still be authored through scene JSON or
rendercli.
`Render -> Preview Engine -> Use Scene Render Settings`
compiles the live preview from that saved intent. Choosing a preview engine,
preview view, overlay, shadows, or preview FXAA/SMAA switches the preview into
an explicit override mode, layering temporary request overrides without
rewriting the scene file. FXAA/SMAA apply to the selected preview executor;
rasterizer preview shadows switch the live preview to Rasterizer before
recompiling because the shadow pass is raster-specific. When the scene intent
does not name a default camera, Modeler annotates scene-rendering passes with
the active scene camera id from the editable scene.
`Render -> Preview
View` can also switch the live graph preview from beauty to depth, stencil,
normal, object-id, material-id, world-position, or raster counter AOVs; the
graph recompiles to show the corresponding AOV producer and visualization
nodes. Raster preview AOVs are backed by rasterizer diagnostics or a raster
stencil-marking pass, so their images match raster tessellation, sampling, and
clipping. Selecting a raster counter preview switches the preview executor to
Rasterizer because those diagnostics measure raster work. `Render -> Preview
Tonemap` selects the operator used by the graph's tonemap node.

`Render -> Render` opens the final render window. Its Graph tab compiles the
final render plan before the Render button starts execution. The plan starts
from the scene render intent, then applies the render-window controls as
temporary final-render overrides such as engine, resolution, samples, raster
MSAA, and shadow-map quality. The image render executes the graph shown in that
tab, including pass toggles made in the graph inspector.

The Render Graph dock compiles the current preview intent into a
[`RenderPlan`](../render-graph/render-plans-and-resources.md) before preview
renders begin. The Graph tab is the primary view: it shows pass nodes,
resource nodes, and dependency edges, supports double-click pass toggles, and
drives the property editor when a pass or resource is selected. Pass and
resource nodes use human-readable display names in the graph while stable ids
remain available in tooltips and exports. Pass nodes show non-default scene
selector, camera, and shading-profile intent directly in the graph and elide
long labels inside the node bounds. Its Passes tab shows each compiled pass
display name, execution order, execution stage, kind, executor, scene selector,
camera reference, shading profile, and resource edges. Its Resources tab shows
each declared resource's display name, type, format, domain, lifetime, and
dimensions. Selecting a pass also shows its execution stage, order, incoming
dependencies, and outgoing dependencies in the property editor alongside pass
state, scene view, shading profile, resource edges, and trace metadata.
Hovering a pass or resource node summarizes its scene-view intent and declared
graph edges without leaving the graph view.
Unchecking a pass adds a graph override and the dock validates the
manipulated plan immediately. When the manipulated plan is still valid, the
central preview renders through that effective plan. After a render, selecting
a pass or inspectable color/depth resource in the graph opens the central Graph
Trace preview tab with input, output, and difference images from the last
execution when that trace still matches the current plan and preview inputs.
The graph nodes themselves summarize pass status/timing and resource
preview/cache status from that same trace.
When a selected resource has no captured image, the trace preview distinguishes
resources missing from the executed plan from resources that were declared but
not read or written by the last execution path.
The Groups tab applies the same override system to every pass matching a
present pass kind, executor, or feature tag.
Resource selections also show trace cache status in the property editor.
For graph-visible raster preview shadows, that status distinguishes a rebuilt
full shadow-map artifact from one restored from the graph artifact cache.
The dock can also export the effective plan as text, DOT, or JSON for the same
inspection/replay workflows as rendercli.

Scenes with a top-level `animation` block enable the Timeline dock. Its slider
and spinbox choose the current frame. The central preview and render dialog
evaluate a copied scene at that frame before building runtime render objects,
so animated camera poses, transforms, colors, and lights are visible in the
editor. When the scene has an active camera, changing frames resets the
central preview to that evaluated camera pose; mouse-drag preview controls can
then move from the keyed pose. The scene tree and property editor remain
attached to the unevaluated authoring scene.

## <a id="scene-files"></a>Scene files
Reusable scene JSON files live under [`scenes/`](../../../scenes/). They are
ordinary world-scene files, so both `rendercli` and `Modeler` load the same
data. The current checked-in scenes cover camera demos, depth of field,
animation frame evaluation, camera panning, light sweeps, material fades,
motion-blur velocity sweeps, visibility-step timelines, transparent materials,
reflections, raster material previews, render-graph AOV and stencil-composite
demos, and small geometry fixtures used by tests.

[`scenes/render_graph_aov_demo.json`](../../../scenes/render_graph_aov_demo.json)
is a focused Modeler graph-inspection scene. Its saved render intent asks for a
rasterizer beauty preview, an SMAA postprocess pass, and a stencil AOV side
branch, so opening the Render Graph dock immediately shows both the main color
chain and an auxiliary resource branch.
[`scenes/render_graph_stencil_composite_demo.json`](../../../scenes/render_graph_stencil_composite_demo.json)
opens with the rasterizer preview and Stencil Composite view selected from its
saved render intent. The compiler synthesizes raster beauty, wireframe beauty,
stencil AOV, composite, tonemap, and exported stencil-preview nodes; the scene
does not name those nodes directly.
When a scene's render intent is ahead of the current compiler, Modeler reports
the graph compile error in the Render Graph dock and pauses the live preview
instead of drawing from a stale plan.

The Modeler does not bake scene catalogs into C++; it opens scene JSON files
directly and routes external model formats through registered
`world::SceneImporter` implementations. Its Open dialog builds the default
scene/import filter from registered importer extensions, so new importers become
selectable without hand-editing the dialog. Successful opens and scene saves are
remembered in `File -> Open Recent`, capped to the ten most recent scene/import
files. LDraw `.ldr`, `.dat`, and `.mpd` imports build a new scene shell on a
worker thread, use the importer's default library-root lookup, and frame a
front-facing camera around the compiled model on a white product-view
background. OpenSCAD `.scad` imports use the same standalone-scene defaults when
opened directly: the imported Z-up asset is oriented upright for the
product-view camera, lit with ambient fill, and framed with a pinhole camera
before the preview starts. `File -> Import` is the same importer path used as an
additive scene operation: it inserts the imported root into the current Elements
tree without replacing the scene or changing the current camera, background,
lights, render settings, or timeline. Direct
OpenSCAD opens and imports remain source-backed as `SourceAsset` objects;
Customizer-style sections, comments, numeric values, booleans, string choices,
and vector expressions appear in the property inspector and rebuild the
generated mesh when edited. Scene animation tracks can target the same editable
source parameters, so evaluated frames rebuild the source-backed output with the
sampled Customizer values. The same source asset exposes a normal material
reference, so assigning a scene material to the asset overrides the generated
OpenSCAD mesh without exposing transient importer children in the Elements tree.
New reusable demos should be added as scene files unless they need a new runtime
feature, a new world wrapper type, or a dedicated importer.

## <a id="the-wireup"></a>The wireup
```text
Scene JSON
       |
       |  loaded by world wrappers
       v
world::Scene
       |
       |  optional frame evaluation, then runtime conversion
       v
render::Scene + render::Camera + render::Tonemap
       |
       |  passed to a chosen RenderEngine subclass
       v
engine::raytracer::Raytracer / engine::raster::Rasterizer / engine::wireframe::Wireframe
       |
       |  render(buffer)
       v
Buffer<unsigned int> -> PNG file (rendercli) or QImage paint (Modeler)
```

Every front end is a different way of producing the inputs on the left and
consuming the output on the right. The chain in the middle is what the ray,
scene-structure, and rasterization chapters cover.

## <a id="exercises"></a>Exercises
1. Predict what changes in the middle of the diagram when the user toggles the
   Modeler preview from Raytracer to Wireframe. What persists across the swap?
2. Run `rendercli --engine raster --msaa 4` on a scene with sharp edges, then
   again with `--msaa 1`. Diff the two output PNGs and identify which pixels
   differ.
3. Open `scenes/animation_frame_demo.json` in the Modeler and scrub the
   Timeline dock. Which scene data is edited by the property editor, and which
   scene data is only evaluated for preview?

## See also

- Volume index: [Tools & I/O](README.md)
- Previous: [PLY parsing](ply-parsing.md)
- Engines used:
  [The Whitted pipeline](../ray-rendering/the-whitted-pipeline.md),
  [The rasterization pipeline](../rasterization/the-rasterization-pipeline.md),
  [Wireframe rendering](../rasterization/wireframe-rendering.md)
- [Top-level TOC](../README.md)

## Source anchors

<!-- source-anchors -->
- `tools/rendercli/`
- `test/rendercli/StepOptionTest.cmake`
- `test/rendercli/RenderGraphOptionTest.cmake`
- `src/modeler/MainWindow.cpp`
- `include/engine/graph/RenderGraphRequest.h`
- `src/modeler/`
- `include/widgets/world/RenderGraphInspectorWidget.h`
- `include/widgets/world/RenderGraphTracePreviewWidget.h`
- `src/widgets/world/RenderGraphInspectorWidget.cpp`
- `src/widgets/world/RenderGraphTracePreviewWidget.cpp`
- `scenes/`
<!-- /source-anchors -->
