# 26. The example apps

The library is the renderer. The library is also useless on
its own — a renderer with no front end produces no images.
This chapter is a tour of the five front ends the codebase
ships, what each one is for, and how each one wires the
engine and scene abstractions together to actually run.

This is a short chapter. The example apps are mostly Qt
plumbing on top of abstractions covered in earlier chapters,
and the per-app documentation lives in the apps' own source.
The point of including them in the textbook is to show how
the library *gets used*, so a reader who has finished
Volumes I–V has a concrete map of where to look when they
want to see a real run.

By the end of this chapter you should know:

- the five example apps and the use case each one serves,
- the engine-selector wiring that lets all three rendering
  engines drop in interchangeably,
- how to add a new built-in scene to the apps that have
  scene catalogs.

## 26.1 `rendercli` — the headless renderer

[`tools/rendercli/`](../../../tools/rendercli/) is a
command-line front end. It reads a JSON scene file, builds the
scene graph through the
[`world::`](../../../include/world/) wrapper layer, configures
a render engine according to command-line flags, runs one
render, and writes a PNG to disk. Then it exits.

The typical invocation:

```sh
$ rendercli --engine raytracer --width 1920 --height 1080 \
            --scene examples/GeneratedRayTracer/scenes/spheres.json \
            --output spheres.png
```

The flags cover everything the apps with GUI knobs cover:
engine choice (`raytracer` / `raster` / `wireframe`), output
size, sampler choice, samples-per-pixel, recursion depth,
tonemap operator, and per-engine knobs ([LOD](../appendix/a-glossary.md#l) for wireframe and
raster, [MSAA](../appendix/a-glossary.md#m) for raster, queue size and thread count for
raytracer).

If the scene has a top-level `animation` block, `--frame N`
evaluates the world scene at frame `N` before the runtime
render scene and active camera are built:

```sh
$ rendercli --engine raster --frame 24 \
            examples/GeneratedRayTracer/scenes/animation_frame_demo.json \
            frame_0024.png
```

The same flag is valid for static scenes; it simply leaves the
loaded scene unchanged.

`--animation` renders the scene timeline as an image sequence. The
output path must contain one printf-style integer placeholder:

```sh
$ rendercli --engine raster --animation --frame_start 1 --frame_end 48 \
            examples/GeneratedRayTracer/scenes/animation_frame_demo.json \
            frames/frame_%04d.png
```

Without explicit frame-range overrides, `rendercli` uses the
scene's `animation.startFrame` and `animation.endFrame` values.

`rendercli` is the right front end for:

- **Headless rendering** — CI machines, remote servers, Docker
  containers without a display server. The interactive apps
  need a Qt event loop and at minimum a window, even if you
  don't see it; `rendercli` runs cleanly without one.
- **Batch rendering** — scripted sweeps over many scenes or
  many parameter values. Wrap `rendercli` in a shell loop and
  produce hundreds of images.
- **Doc renders** — the
  [`scripts/docs/`](../../../scripts/docs/) Ruby drivers all
  invoke `rendercli` to produce the per-class image sweeps
  that show up in Doxygen and (forwarded) in this book's
  rendered figures.
- **Performance testing** — `--timing` reports the render
  time excluding setup; `--repeat N` runs the same render N
  times and reports min/median/avg/max. Used for the
  rasterizer benchmark numbers in
  [chapter 21 §21.5](../04-rasterization/21-msaa-and-attribute-interpolation.md#21-5-the-cost-of-msaa).

## 26.2 `examples/GeneratedRayTracer` — the interactive editor

[`examples/GeneratedRayTracer/`](../../../examples/GeneratedRayTracer/)
is the closest thing the codebase has to a 3D modeling app.
It loads a JSON scene the same way `rendercli` does, but
displays it in a Qt window with a property editor on the
right, a scene-tree outliner on the left, and a live preview
in the center. The user can mouse-drag to rotate the camera,
edit primitive parameters in the property editor and see the
result re-render, swap the engine between Raytracer / Rasterizer /
[Wireframe](../appendix/a-glossary.md#w) via a dropdown, and trigger final-output renders to
file via a render dialog.

The `Display` widget — the central preview — is a subclass of
[`QtDisplay`](../../../src/widgets/QtDisplay.cpp), which is
itself a subclass of
[`RenderWidget`](../../../src/widgets/RenderWidget.cpp). The
inheritance chain reflects the responsibilities:
`RenderWidget` owns the framebuffer pair and the render-job
lifecycle; `QtDisplay` adds mouse-drag camera control;
`GeneratedRayTracer::Display` adds the engine-selector swap and
the Ctrl-click ray-state probe.

`RenderWidget` keeps a UI-thread *front* image plus a *back*
buffer per render job. Worker threads write into the back
buffer; the widget's `paintEvent` only ever draws the
immutable front snapshot. The display mode chooses when pixels
flow from back to front:

- `PeriodicUpdate` copies the whole back buffer at every timer
  tick, including in-flight tile writes. The historic
  partial-output behavior; useful for the raytracer where
  watching tiles fill in is informative.
- `CompletedTilePublishing` copies only engine-reported
  completed tiles. Avoids reading tiles still being written.
- `DoubleBuffer` does not publish in-flight pixels at all; it
  swaps front and back when the render thread finishes. The
  natural choice for the rasterizer and wireframe engines,
  which are fast enough that partial output isn't useful.

Engines that implement `RenderEngine::cloneForRender()` get a
snapshot per render job, which lets an interactive preview
cancel an old job and start the replacement immediately while
the old worker drains in the background. Engines that return
`nullptr` from `cloneForRender` keep the serialized lifecycle
— the widget waits for the current render to finish before
starting the next one.

The `Display::applyPreviewPolicy(kind)` helper picks one of
two policies depending on the active engine. The raytracer
runs `PeriodicUpdate` with a 16 ms publish tick and cancels
the in-flight render when the camera moves, so users see
partial output that snaps to the new view immediately. The
rasterizer and wireframe engines run `DoubleBuffer` with no
publish tick and no cancel-on-interaction; they finish each
frame before swapping it in, since both engines render fast
enough that partial output isn't useful and a swap-on-complete
animation reads as smoother than a tile-by-tile fill.

The engine selector is the most interesting piece of wiring.
The `Display` holds three engine pointers — one for each kind
— and on a kind switch updates the active one to share the
scene and camera with the previously-active one, so the
preview keeps looking at the same thing across the swap. The
live rasterizer preview keeps shadow maps off by default for
interaction speed, but the render menu exposes a preview-only
shadow toggle that enables four stabilized directional-light
cascades for inspecting shadow-map behavior while editing.

Scenes with a top-level `animation` block enable the Timeline
dock. Its slider and spinbox choose the current frame. The
central preview and render dialog evaluate a copied scene at
that frame before building runtime render objects, so animated
camera poses, transforms, colors, and lights are visible in
the editor. The scene tree and property editor remain attached
to the unevaluated authoring scene.

`GeneratedRayTracer` is the right front end for:

- **Scene authoring** — building a scene by hand, with the
  property editor giving immediate feedback on each parameter
  change.
- **Final-output rendering** — the render dialog opens a
  separate window that renders at higher resolution and with
  more samples than the preview.
- **Engine debugging** — the side-by-side comparison of
  Raytracer / Rasterizer / Wireframe outputs is the fastest way
  to catch tessellation bugs (Wireframe shows the topology
  directly), shading bugs (Raytracer-vs-Rasterizer
  divergence), or material bugs (each engine handles materials
  slightly differently).

## 26.3 `examples/SceneBrowser` — interactive scene picker

[`examples/SceneBrowser/`](../../../examples/SceneBrowser/) is
the more lightweight interactive app. It carries a built-in
catalog of scenes — each one a C++ file under
`examples/SceneBrowser/<Name>Scene.cpp` — and presents a
dropdown to select among them. Same `QtDisplay` mouse-drag
camera control, no property editor, no scene-tree outliner.
The user picks a scene from the dropdown and explores it.

This is the right front end for:

- **Demo / exploration** — showing what the renderer can do
  to a curious viewer who just wants to drag the camera
  around.
- **Built-in test scenes** — scenes that don't fit the JSON
  scene-graph model (procedurally generated, parametric,
  tied to specific code paths) live as
  `<Name>Scene.cpp` files and show up in the dropdown.
- **Sidebar parameter widgets** — when a scene's interesting
  parameter (camera focal distance, sampler count) wants a
  visible knob, the scene's accompanying parameter widget
  registers via `<Camera>ParameterWidgetFactory` and shows up
  in the right-hand sidebar.

## 26.4 The other two: `DifferenceRayTracer`, `RefractingRayTracer`

[`examples/DifferenceRayTracer/`](../../../examples/DifferenceRayTracer/)
and
[`examples/RefractingRayTracer/`](../../../examples/RefractingRayTracer/)
are smaller targeted examples — each one a single scene
showcasing a specific feature.

`DifferenceRayTracer` shows [CSG](../appendix/a-glossary.md#c) difference operations: a
sphere with smaller spheres carved out, demonstrating the
[chapter 14](../03-scene-structure/14-csg.md) interval-set
operations on a real render.

`RefractingRayTracer` shows transparent materials: glass
spheres, refraction with critical-angle total internal
reflection, the multi-bounce recursion at the heart of
[chapter 8 §8.4](../02-ray-rendering/08-materials-and-brdfs.md#8-4-the-five-shipped-materials).
The interesting feature is its **debug visualization** — it
uses
[`render::State`](../../../include/render/State.h)'s event
log to draw the actual paths rays take through refractive
geometry, not just the final pixel output. Useful when
you're debugging a specific scene's behavior.

Both apps are essentially specialized `SceneBrowser`
variants — one scene baked in, no dropdown — kept around as
focused demos for the techniques they showcase.

## 26.5 Adding a built-in scene

Adding a scene to `SceneBrowser` is two files plus a
registration line:

1. **The scene class.** Subclass
   [`SceneFactory::Scene`](../../../examples/SceneBrowser/) —
   override `name()` to return the dropdown label, and
   `build()` to return a `std::shared_ptr<render::Scene>`
   carrying the populated scene graph.
2. **The scene file.** Under
   `examples/SceneBrowser/<Name>Scene.cpp`. The class lives
   here.
3. **The registration.** A static initializer in the same
   file calls `SceneFactory::self().registerClass<MyScene>()`,
   which adds the scene to the dropdown automatically. No
   GUI code changes.

Following the existing scenes as templates is the easiest
path — most of them are 50 to 100 lines of scene-graph
construction.

## 26.6 The wireup, in one diagram

```
Scene description (C++ or JSON)
       │
       │  loaded by world:: wrappers (or built in code)
       ▼
render::Scene  +  render::Camera  +  render::Tonemap
       │
       │  passed to a chosen RenderEngine subclass
       ▼
engine::raytracer::Raytracer  /  engine::raster::Rasterizer  /  engine::wireframe::Wireframe
       │
       │  render(buffer)
       ▼
Buffer<unsigned int>  →  PNG file (rendercli) or QImage paint (interactive apps)
```

That is the whole library, viewed from the application side.
Every front end is a different way of producing the inputs
on the left and consuming the output on the right; the chain
in the middle is what Volumes II, III, and IV cover.

## 26.7 Exercises

1. Predict what changes in the middle of the diagram when the
   user toggles the engine selector in `GeneratedRayTracer`
   from Raytracer to Wireframe. What persists across the
   swap? What gets thrown away?
2. Run `rendercli --engine raster --scene <some scene> --msaa 4`
   on a scene with sharp edges, then again with `--msaa 1`.
   Diff the two output PNGs and identify which pixels differ.
   Explain the difference in terms of
   [chapter 21 §21.3](../04-rasterization/21-msaa-and-attribute-interpolation.md#21-3-msaa-coverage-sampling-not-shading-sampling).
3. Write a new `SceneBrowser` scene that demonstrates a torus
   with a mirror surface. What scene-graph code is needed? Is
   any new C++ or runtime code needed beyond the scene file
   itself?
4. The interactive apps construct a `QApplication` in their
   `main`, which `rendercli` doesn't. Why does `rendercli`
   need to construct a `QCoreApplication` instead? What would
   happen if it constructed neither?

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
- `examples/GeneratedRayTracer/`
- `examples/SceneBrowser/`
- `examples/DifferenceRayTracer/`
- `examples/RefractingRayTracer/`
<!-- /source-anchors -->
