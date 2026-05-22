# 26. Tools and the Modeler

The library is the renderer. The checked-in front ends are the
small command-line renderer and the Qt modeling UI that turns scene
files into something visible and editable.

By the end of this chapter you should know:

- how `rendercli` loads and renders scene JSON,
- how the `Modeler` executable wires the shared render engines into a GUI,
- where reusable scene JSON files live.

## 26.1 `rendercli` — the headless renderer

[`tools/rendercli/`](../../../tools/rendercli/) is a command-line front end.
It reads a JSON scene file, builds the scene graph through the
[`world::`](../../../include/world/) wrapper layer, configures a render engine
from command-line flags, runs one render, and writes a PNG to disk.

The typical invocation:

```sh
$ rendercli --engine raytracer --width 1920 --height 1080 \
            scenes/dice.json \
            dice.png
```

The flags cover engine choice (`raytracer` / `raster` / `wireframe`), output
size, sampler choice, samples-per-pixel, recursion depth, tonemap operator,
and per-engine knobs such as [LOD](../appendix/a-glossary.md#l), [MSAA](../appendix/a-glossary.md#m),
queue size, and thread count.

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

## 26.2 `src/modeler` — the interactive editor

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
- `src/modeler/Display.cpp` adds engine selection, rasterizer-preview shadow
  policy, and the Ctrl-click ray-state probe.

The editor can swap the live preview between Raytracer, Rasterizer, and
[Wireframe](../appendix/a-glossary.md#w). On a kind switch it updates the new
engine to share the scene and camera with the previous one, so the preview
keeps looking at the same thing across the swap.

Scenes with a top-level `animation` block enable the Timeline dock. Its slider
and spinbox choose the current frame. The central preview and render dialog
evaluate a copied scene at that frame before building runtime render objects,
so animated camera poses, transforms, colors, and lights are visible in the
editor. The scene tree and property editor remain attached to the unevaluated
authoring scene.

## 26.3 Scene files

Reusable scene JSON files live under [`scenes/`](../../../scenes/). They are
ordinary world-scene files, so both `rendercli` and `Modeler` load the same
data. The current checked-in scenes cover camera demos, depth of field,
animation frame evaluation, motion blur, transparent materials, reflections,
and small geometry fixtures used by tests.

The Modeler does not bake scene catalogs into C++; it opens JSON scene files
directly. New reusable demos should be added as scene files unless they need a
new runtime feature or a new world wrapper type.

## 26.4 The wireup

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
consuming the output on the right. The chain in the middle is what Volumes II,
III, and IV cover.

## 26.5 Exercises

1. Predict what changes in the middle of the diagram when the user toggles the
   Modeler preview from Raytracer to Wireframe. What persists across the swap?
2. Run `rendercli --engine raster --msaa 4` on a scene with sharp edges, then
   again with `--msaa 1`. Diff the two output PNGs and identify which pixels
   differ.
3. Open `scenes/animation_frame_demo.json` in the Modeler and scrub the
   Timeline dock. Which scene data is edited by the property editor, and which
   scene data is only evaluated for preview?

## See also

- Volume index: [Volume VI — Tools & I/O](README.md)
- Previous: [25. PLY parsing](25-ply-parsing.md)
- Engines used:
  [5. The Whitted pipeline](../02-ray-rendering/05-the-whitted-pipeline.md),
  [18. The rasterization pipeline](../04-rasterization/18-the-rasterization-pipeline.md),
  [20. Wireframe rendering](../04-rasterization/20-wireframe-rendering.md)
- [Top-level TOC](../README.md)

## Source anchors

<!-- source-anchors -->
- `tools/rendercli/`
- `src/modeler/`
- `scenes/`
<!-- /source-anchors -->
