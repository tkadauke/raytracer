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
controls such as MSAA, post-process AA, viewport/scissor state, color-output
state, and shadow-map settings are compiled into typed raster beauty pass state
and serialized when graph JSON is exported.

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
that as the graph compiler's base intent.
When compiling a plan, `--render_graph_executor raytracer|rasterizer|wireframe`
overrides the graph intent's default executor, and
`--render_graph_view default|beauty|wireframe` overrides the graph intent's
structural view mode. `--render_graph_wireframe_overlay` asks the compiler to
insert a graph-visible wireframe overlay pass between the beauty pass and the
tonemap pass.
For raster graph renders, `--post_aa fxaa` and `--post_aa smaa` are also
graph-visible: rendercli inserts a postprocess pass after `raster_beauty`
rather than hiding the filter inside the rasterizer engine, and the pass's
typed `post_process_aa` parameters select the replayed filter. `--post_aa
taa` stays on the raster beauty pass until temporal history resources are
graph resources.
Wireframe graph renders carry `--lod` in typed wireframe pass state, so
graph-only JSON exports and replayed graph renders preserve the requested
tessellation density.
`--disable_pass`, `--disable_pass_kind`, `--disable_executor`, and
`--disable_feature` apply graph overrides before validation or rendering.
Those controls are intentionally graph-level: disabling the required
`raytrace_beauty` pass, for example, makes validation fail before any image is
written. Replaying a saved graph uses the exported color resource dimensions,
so `--width` and `--height` only need to be supplied when they intentionally
match the saved plan.

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
thing across the swap. `Render -> Preview Tonemap` selects the operator used by
the graph's tonemap node.

The Render Graph dock compiles the current preview intent into a
[`RenderPlan`](../render-graph/render-plans-and-resources.md) before preview
renders begin. Its Passes tab shows each compiled pass id, kind, executor, and
resource edges. Its Resources tab shows each declared resource's type, format,
domain, lifetime, and dimensions. Unchecking a pass adds a graph override and
the dock validates the manipulated plan immediately. When the manipulated plan
is still valid, the central preview renders through that effective plan.

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
reflections, raster material previews, and small geometry fixtures used by
tests.

The Modeler does not bake scene catalogs into C++; it opens JSON scene files
directly. New reusable demos should be added as scene files unless they need a
new runtime feature or a new world wrapper type.

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
- `test/rendercli/RenderGraphOptionTest.cmake`
- `src/modeler/`
- `include/widgets/world/RenderGraphInspectorWidget.h`
- `src/widgets/world/RenderGraphInspectorWidget.cpp`
- `scenes/`
<!-- /source-anchors -->
