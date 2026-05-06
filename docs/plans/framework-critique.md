# Framework critique — May 2026

> **Scope:** Honest critique of the in-repo frameworks I touched while
> shipping `ThinLensCamera` and `EquirectangularCamera`. Each section has
> what works, what hurt at the moment of touching it (with concrete
> session evidence), and where I'd take it next. Companion doc to
> `roadmap.md` and `topics-backlog.md`.
>
> **Status:** Living document — items here graduate to the roadmap when
> picked up, or get pruned if disproven.

---

## 1. Doc-render framework

`scripts/render_docs.rb` + `scripts/lib/scene.rb` + `scripts/docs/*.rb`

### What works

- DSL is concise — one .rb file per feature gives `class_doc` plus
  parameter sweeps.
- Helpers like `camera_scene` / `dof_scene` / `panorama_scene` compose
  cleanly.
- Reuses `rendercli` as the renderer, so doc images come from the same
  pipeline that ships. A render bug surfaces in the docs (not great
  when it happens, but means there's no second renderer to keep in
  sync).
- Color helpers (`red`, `green`, `blue`, `yellow`) are reusable and
  ergonomic.

### What hurt during this session

- **Hardcoded sizes in `class_doc` and `property_doc`.** `class_doc`
  always renders 640×480 (4:3); `property_doc(5)` always renders
  240×180. Bit me on `EquirectangularCamera`, which needs a 2:1
  aspect — I had to drop down to `doc_scene` directly. Anything
  panoramic, anamorphic, or unusually tall (cylindrical, tilt-shift,
  ODS stereo) will hit the same wall.

- **IEEE 754 float-to-filename traps.** `(i - 1) * 0.1` for `i = 4`
  produces `0.30000000000000004` in double precision, and Ruby's
  `"#{...}"` prints all the digits. The Doxygen `@image html`
  reference expects `0.3.png`. Workaround in
  `thin_lens_camera.rb`: hardcode parameter values as strings. The
  Doxygen ref and the doc-render driver have to agree exactly, with
  no compile-time check that they do.

- **The Ruby DSL classes in `scripts/lib/scene.rb` are a parallel
  hierarchy to the C++ Q_PROPERTY classes.** Adding a new world
  class means manually mirroring it in `scene.rb`. Drift hazard —
  there's probably already drift somewhere; the only way to find it
  is "render breaks weirdly."

- **`scripts/lib/colors.rb` had `blue` assigning to `@green`** for who
  knows how long. No test caught it because there are no tests for
  the doc-render framework, and visually only a render that uses
  *both* `green()` and `blue()` exposes it. I introduced
  `panorama_scene` which uses both and the bug surfaced as "two of my
  three coloured spheres rendered green."

- **No staleness detection.** `--missing` only checks file existence.
  If you tweak `panorama_scene` to add a fifth sphere but the
  existing PNGs already exist, nothing re-renders. Manual
  `rm docs/images/<feature>*.png` before `rake docs:render` is the
  workaround. Better: a content hash on the JSON sent to rendercli,
  included in the filename or stored alongside, so re-renders auto-
  trigger on driver changes.

- **The image-filename ↔ Doxygen-`@image-html` coupling is stringly
  typed across two files in two languages.** A rename in the .rb file
  silently breaks Doxygen rendering; only spotted by visual inspection
  of the docs.

### Where I'd take it

- **Quick win**: extend `render_size` to accept aspect-ratio overrides
  (`render_size(5, aspect: :panoramic)` → 2:1). One day's work.
- **Medium**: add a content-hash-based staleness check so
  `rake docs:render` knows to re-render when the driver changes. Plus
  a CI check that every `@image html` reference in headers points to a
  file the doc-render produces — would catch typos and broken renames
  automatically.
- **Strategic**: code-generate the `scripts/lib/scene.rb` Ruby DSL
  from the C++ Q_PROPERTY metadata. Eliminates the parallel-hierarchy
  drift entirely. Day-to-week scoped.

---

## 2. Interactive widgets framework

`scripts/docs/figure.js` + `scripts/docs/*.js` (19 widgets)

### What works

- Self-contained — no React, no D3, no build step. Just SVG via DOM.
- Each widget is 50-150 lines, all follow the same pattern: a class
  with `createCanvas`, bound to a `DragHandler`. Read one and you can
  read them all.
- Direct DOM rendering — no virtual-DOM overhead matters across 19
  widgets in the docs.
- `DragHandler` is a clean encapsulation of mouse logic.
- SVG output works in any browser without polyfills.

### What hurt during this session

- **Custom `Class()` factory** at lines 59-86 of `figure.js` predates
  ES6 classes by years. `var` everywhere, no `let` / `const`, no
  `extends` / `super`. Modernizing is mostly mechanical but would
  clean up a lot of boilerplate.

- **Vector and primitives are 2D-only.** Fine for the existing
  Angle/Ray/BoundingBox widgets and the new `ThinLens` convergence
  widget (which lives in a side-view 2D plane). But anything genuinely
  3D — say a "see how the `Vector3d` cross product works" widget —
  would need new infrastructure. The 3D rendering is *exactly the kind
  of thing this raytracer is built for*; not having it in the doc
  widgets is ironic.

- **One interaction model: drag-horizontally.** No sliders with snap,
  no buttons, no keyboard navigation, no touch support. That's a lot
  of "click to drag, hope you guess what it does." A "reset to
  default" button on every widget would help; a small slider widget
  primitive would too.

- **Global CSS injection** — `figure.js` prepends a `<style>` to
  `<head>`. All widgets on a page share the same styles. Adding
  widget-specific styling (e.g. blue rays in the convergence widget vs
  red rays elsewhere) requires fragile class-name discipline.

- **Zero tests.** A subtle change to `Vector.rotated` could break
  every Angle widget and there's no automated check. For an education-
  focused repo where the widgets *are* the educational content, that's
  a problem.

- **No declarative-shape system.** Want a parabola, a Bézier, a
  function plot? Everything is line / circle / rect. A `Path(d)`
  primitive would be a small addition with big payoff.

- **`document.scripts[document.scripts.length - 1]`** is the standard
  "anchor to my own `<script>` tag" trick but it's fragile if Doxygen
  ever changes how it embeds JS. A `<div data-widget="thin_lens_camera_convergence">`
  placeholder that the script finds by name would be more robust.

### Where I'd take it

- ✅ **Quick win**: a `Path(d)` primitive plus a `Slider({...})` UI
  primitive. Done — see commit log. The two ThinLens widgets
  migrated to use `Slider` as the worked example.
- ✅ **Better widget anchoring**: `document.currentScript` instead
  of `document.scripts[length-1]`. Done in the same commit.
- ✅ **Tests**: `scripts/test/test_figure_js.js` using Node's
  built-in `node:test` runner with a minimal DOM shim — no new
  dependencies. 19 cases covering Vector math, Class factory, and
  the structural correctness of Path / Slider output. Added to
  `rake test:scripts:js`.
- ✅ **Medium**: ES6 modernisation. `figure.js` rewritten using
  native `class` syntax for every primitive (Vector, Canvas, Group,
  Line, Ray, Circle, Rectangle, Text, Axes, Path, Slider,
  DragHandler, OrderedHash). `var` → `const`/`let`, arrow functions
  in event handlers, template literals, default parameters,
  destructured Slider options. **All 20 widgets** migrated to the
  modern syntax (the two ThinLens widgets first as worked examples,
  then all 18 pre-existing Angle / Ray / BoundingBox / ConvexHull /
  Hitpoint / Sphere / Box widgets). The `Class()` factory is
  preserved as a 30-line compatibility shim — covered by unit
  tests, but no in-repo widget actually uses it any more. Smoke
  test loads every widget in a shared sandbox to catch shim
  regressions and broken cross-widget inheritance.

  Side benefits surfaced during the migration:
  - Several typo'd class names (`BoudingBoxClass` → `BoundingBoxClass`,
    five other `BoudingBox*` variants, `RayClass` collision between
    `ray_class.js` and `ray_at.js`) corrected. They were JS-internal
    symbols only, never referenced from C++ headers.
  - `clamp` global pollution in `ray_at.js` (originally a top-level
    `function clamp() { ... }`) replaced with a per-file `const`
    scoped lexical, so multi-widget pages can't pick up the wrong
    helper.
- **Medium (deferred)**: scoped CSS. Currently `figure.js` injects a
  global stylesheet into `<head>`; a per-widget styling option would
  require either Shadow DOM or a CSS-class-namespacing convention.
  No widget has needed widget-specific styling yet.
- **Strategic (parked)**: a `Scene3D` primitive backed by a tiny
  WebGL / three.js renderer for genuinely 3D illustrations. The
  repo's whole identity is 3D rendering; having 3D doc widgets that
  match the topic would be high-value. But three.js is a real
  dependency, and **no concrete consumer demands 3D today**: the
  ThinLens convergence widget is faithfully a 2D side-view; the
  lens-disc sampling is genuinely 2D; Angle/Ray/BoundingBox are 2D
  by nature. Revisit when there's a specific widget that bottlenecks
  on 2D-only.

---

## 3. `world::Element` / `Factory` / `Q_PROPERTY` metasystem

### What works

- Clean split between editable (`world::`) and runtime (`raytracer::`)
  hierarchies — different responsibilities, justified duplication.
- `Q_PROPERTY` + `PropertyEditorWidget` reflection means new world
  classes get auto-built GUI editors with zero per-class widget code —
  load-bearing piece of the GUI experience and elegant.
- `ElementFactory` + JSON read/write gives free serialization.
- Static-init self-registration keeps adding-a-new-element to a single
  `.cpp` file.

### What hurt during this session

- **`Q_DECLARE_METATYPE` is per-translation-unit, and
  `qRegisterMetaType` is per-process-startup.** `Element.cpp` declares
  the three numeric types it cares about; `ScriptedSurface.cpp`
  declares two; every widget `.cpp` re-declares its own; and rendercli
  / GeneratedRayTracer / every test that round-trips properties each
  repeats the `qRegisterMetaType` list. **Forgetting the registration
  is a silent failure** — `QMetaProperty::read` returns an invalid
  `QVariant` and the JSON roundtrip drops the property without
  warning. I noted this as a TODO in `SceneTest.cpp` during the
  World/* test pass.

- **Stringly-typed factory keys must match `metaObject()->className()`
  exactly**, including namespace qualification for inline classes.
  Caught me in `SceneTest` where `TestElement` got serialized as
  `"ElementTest::TestElement"` not `"TestElement"`. No compile-time
  check that the registration string matches the class — drift hazard.

- **Two parallel parameter-editor systems.** World classes get auto-
  built editors via `PropertyEditorWidget` (Q_PROPERTY-driven);
  runtime cameras get hand-rolled widgets via `CameraParameterWidget`
  + `.ui` files (each one is 4-5 files of boilerplate as I just
  experienced building `ThinLensCameraParameterWidget`). The
  `CameraParameterWidget` hierarchy could be retired in favour of
  using `PropertyEditorWidget` against the world wrapper.

### Where I'd take it

- **Quick win**: lift `qRegisterMetaType` into a library-level init
  function (`world::registerMetaTypes()` or similar). Mechanical
  refactor, eliminates a class of silent bugs.
- **Medium**: replace the `CameraParameterWidget` hierarchy with
  `PropertyEditorWidget` reflection. One world::camera = one
  Q_PROPERTY block = automatic editor. Removes ~6 files of
  boilerplate per new camera type.
- **Strategic**: a code-generated `class name → factory key` mapping
  with compile-time validation, eliminating stringly-typed-key drift.

---

## 4. ViewPlane × Sampler abstraction

(modernize.md §3.4 and topics-backlog §A already touch the broader
sampling theme; this is a focused note from the camera-shipping work.)

### What hurt during this session

- The sampler is 1D-output-only (`std::vector<Vector2d>`) and per-pixel
  iteration is built around that shape. `ThinLensCamera` wanted 4D
  samples (pixel.x, pixel.y, lens.u, lens.v) and had to fake it by
  reusing the sub-pixel offset as the lens coordinate — works but
  introduces correlation. Owen scrambling / Sobol would decorrelate
  properly.

- The factory-default 1-spp `RegularSampler` is a footgun for any
  camera that *needs* multi-sample. The SceneBrowser confetti
  regression on ThinLens was this bug. The auto-install workaround is
  per-camera and stringly couples the camera to a sampler choice — a
  cleaner fix would be a "minimum samples" hint on the camera that
  the framework reconciles with the user's choice.

- The samplers are limited to Regular / Random / Jittered. No quasi-
  Monte Carlo (Sobol, Halton, Owen scrambling). topics-backlog §A
  already flags this as the wanted direction.

### Where I'd take it

- topics-backlog §A is already the right placeholder for the broader
  sampling-theory pillar.
- The framework limitation worth noting: a 4D-sample-shape sampler
  protocol would unlock the whole DOF / motion-blur / light-sampling
  co-stratification space. That's a meaningful refactor of
  `Sampler::generateSet` and the camera render loop, not a drop-in.

---

## 5. Cross-cutting: parallel hierarchies in three places

Three places where the same concept exists in two parallel forms:

| Concept | Form A | Form B |
|---|---|---|
| Cameras | `raytracer::PinholeCamera` (math) | `world::PinholeCamera` (editable) |
| Materials | `raytracer::PhongMaterial` (shader) | `world::PhongMaterial` (editable) |
| Scenes | `scripts/lib/scene.rb` Ruby DSL | C++ Q_PROPERTY classes |

Form A and Form B drift unless every new feature touches both. The
first two are intentional (different responsibilities — runtime maths
vs editable scene-graph). The third (Ruby vs C++) is **gratuitous
duplication** — same data shape, two encodings, manually kept in sync.

### Where I'd take it

Generate the Ruby DSL from the C++ Q_PROPERTY metadata at build time.
The cost of that infrastructure pays off the first time someone forgets
to update `scene.rb` after adding a Q_PROPERTY. Probably day-to-week
scoped depending on how much of the existing DSL needs to migrate.

---

## Recommended ranked priorities

If you spend a session on framework improvement before the next
feature, in descending order of leverage:

1. **Lift `qRegisterMetaType` into `world::registerMetaTypes()`.**
   Small. Removes a real class of silent bugs you've already been
   burned by. ~Half a day.

2. **Add `class_doc` / `property_doc` aspect-ratio overrides.** Small.
   Unblocks any future panoramic / tilt-shift / anamorphic /
   cylindrical / ODS-stereo camera. ~Half a day.

3. **Generate `scripts/lib/scene.rb` from the C++ Q_PROPERTY
   metadata.** Medium. Eliminates the third parallel hierarchy. ~Day-
   to-week.

4. **Replace `CameraParameterWidget` with `PropertyEditorWidget`.**
   Medium. Removes ~6 files of boilerplate from every future camera
   with parameters. ~Day-to-day-and-a-half.

5. **`Path` and `Slider` primitives in `figure.js`.** Small. Opens up
   the widget design space. ~Half a day.

6. **Tests for the doc-render framework.** Small but high-impact —
   would have caught the `blue` / `@green` typo immediately. ~Half a
   day if scoped tightly.

#1 and #2 each are about a half-day. #6 is also about half a day if
scoped tightly. The other three are days-to-week scope.

Hold off on #3 and a WebGL `Scene3D` widget primitive until there's a
concrete driver — building a code-generator for one consumer is rarely
worth it; building it once you have three is a no-brainer.

---

*End of critique. Items here graduate to the roadmap when picked up.*
