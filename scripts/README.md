# `scripts/` — doc-render framework + CLI utilities

This directory contains the Ruby + JavaScript tooling that produces
the documentation images and interactive widgets you see in the
generated Doxygen HTML, plus a few scene scripts and the linter for
keeping `@image html` references in headers in sync with the
on-disk PNGs.

> **For agents / contributors adding a new visible-output feature
> (camera, material, primitive, ...):** read this file *and*
> `CLAUDE.md`'s "Adding a new visible-output feature" checklist.
> The two are complementary: CLAUDE.md tells you *what* to deliver
> (image refs, doc-render driver, demo scene, ...); this file tells
> you *how* the doc-render machinery actually works.

---

## What lives here

| File / dir | What it is |
|---|---|
| `render_docs.rb` | Top-level CLI. Walks `docs/*.rb` and renders each via rendercli. |
| `lib/scene.rb` | The DSL primitives — `Element`, `ElementCreator`, `Scene`, `Transformable`, ..., plus the `scene { ... }` entry point. |
| `lib/colors.rb` | Memoised colour-texture helpers — `red`, `green`, `blue`, `yellow`, `white`, ... |
| `lib/materials.rb` | Memoised material helpers — `red_matte`, `glass`, ... |
| `lib/objects.rb` | Compound object helpers — `checker_board` (the floor box). |
| `lib/lights.rb` | Light helpers — `sunlight` (a directional light). |
| `lib/cameras.rb` | Camera helpers — `default_camera` (a pinhole). |
| `lib/core_ext.rb` | Tiny `String` monkey-patches: `camelize` and `constantize`. |
| `docs/*.rb` | Per-feature doc-render drivers — one .rb file per camera/material/etc. that emits `class_doc` / `property_doc` blocks. |
| `docs/*.js` | Interactive SVG widgets (Angle, Ray, BoundingBox, ThinLens, ...). See "Interactive widgets" below. |
| `lint_doc_images.rb` | Linter that scans headers for `@image html` references and diffs against `docs/images/*.png`. |
| `test/test_*.rb` | Minitest cases pinning the doc-render framework. Run via `rake test:scripts`. |

`brick.js`, `dice.js`, `rotating_sphere.rb`, `spinning_glass.rb` are
*scene* scripts (consumed at runtime by `ScriptedSurface` /
`rendercli`), not doc-render drivers. They live alongside but use a
different code path.

---

## How a doc image gets made

Concretely, when you run `rake docs:render`:

1. Rakefile invokes `ruby scripts/render_docs.rb --missing`.
2. `DocsRenderer#run` globs every `scripts/docs/*.rb` and `eval`s each
   one in the renderer's binding.
3. Each driver script calls `class_doc { ... }` or
   `property_doc(num) { |i| ... }`. These wrap `doc_scene`, which
   wraps the global `scene(opts) { ... }` from `lib/scene.rb`.
4. `scene` builds a `Scene` object via the `ElementCreator`
   metaclass. The driver's block runs in the creator's binding, so
   `pinhole_camera`, `sphere`, `matte_material`, etc. inside the
   block are dynamically resolved (via `method_missing`) to the
   matching Ruby DSL class — `PinholeCamera`, `Sphere`,
   `MatteMaterial` — which gets attached as a child of the scene.
5. After the block runs, `Scene#render`:
   - Computes a SHA1 over the (normalised) JSON + render options
     (see "Staleness detection" below).
   - Compares against the sidecar `<image>.png.hash` file.
   - If unchanged, skips the render. Else writes the JSON to a
     temp file, shells out to `rendercli` with `--width`, `--height`,
     `--sampler`, `--samples_per_pixel`, ..., and writes the
     resulting PNG plus an updated `.png.hash`.

The output PNGs land under `docs/images/`. The C++ headers reference
them via `@image html foo.png` directives that Doxygen resolves to
`docs/html/foo.png` (Doxygen copies them at HTML-generation time).

---

## Adding a new doc-render driver

Say you've added a `KaleidoscopeCamera` and want to ship parameter-
sweep images for the docs. Steps:

1. **Add a Ruby DSL class** in `lib/scene.rb` mirroring the C++
   `Q_PROPERTY` block:

   ```ruby
   class KaleidoscopeCamera < Camera
     property :facets => 6,
              :twist  => 0.0
   end
   ```

   (Ideally this would be auto-generated from the C++ metadata. It
   isn't — see `docs/plans/framework-critique.md`. Just remember to
   keep them in sync.)

2. **Add the driver** at `scripts/docs/kaleidoscope_camera.rb`:

   ```ruby
   class_doc do
     name "kaleidoscope_camera"
     camera_scene
     kaleidoscope_camera :position => [0, -1, -5]
   end

   property_doc do |i|
     facets = 3 + i        # 4, 5, 6, 7, 8
     name "kaleidoscope_camera_facets_#{facets}"
     camera_scene
     kaleidoscope_camera :position => [0, -1, -5], :facets => facets
   end
   ```

   `class_doc` produces one image at the canonical default settings.
   `property_doc { |i| ... }` produces five (default `num`) images
   varying parameter `i` from 1 to 5.

3. **Pick an aspect ratio**. The default is 4:3. If your camera
   needs something else (panoramic, square, anamorphic), pass the
   `aspect:` keyword:

   ```ruby
   class_doc aspect: :panoramic do ... end       # 2:1
   class_doc aspect: :square    do ... end       # 1:1
   property_doc 5, aspect: :panoramic do |i| ... end
   ```

   See `lib/render_size` for the full list. Add a new aspect there
   if you need one and update the test in
   `scripts/test/test_render_size.rb`.

4. **Reference the images from your C++ Doxygen** — see CLAUDE.md
   "Adding a new visible-output feature" §3 for the canonical
   placement.

5. **Verify it works empirically**:

   ```bash
   rake docs:render
   rake check:doc-images   # should report your refs as resolved
   ```

   Or only re-render your driver:

   ```bash
   ruby scripts/render_docs.rb --only kaleidoscope_camera
   ```

---

## Staleness detection

`Scene#render` writes a sidecar `<image>.png.hash` file containing a
SHA1 over (normalised JSON + render options). On the next run with
`--missing`, it recomputes the hash and skips the render if it
matches.

**What counts as "changed"** — i.e. what triggers a re-render:

- Any structural change to the rendered scene (new sphere, different
  material, different camera position).
- A change to the render options (sampler, samples_per_pixel, width,
  height).
- An edit to a helper used by the driver (e.g. `panorama_scene` in
  `render_docs.rb`) that changes the resulting JSON.

**What doesn't count** — i.e. is correctly NOT a re-render trigger:

- The per-run random UUIDs that `Element#initialize` assigns via
  `SecureRandom`. These differ on every run; the hash function
  strips them out before hashing so logically-identical scenes
  produce identical hashes.

If you suspect the staleness check is wrongly skipping, just delete
the `.png.hash` sidecar (or the `.png` itself) and re-run. The
mechanism is local and reversible.

---

## Common gotchas

- **IEEE 754 float-to-filename traps.** `(i - 1) * 0.1` for `i=4`
  produces `0.30000000000000004` in double precision, and Ruby's
  `"#{...}"` prints all the digits. Hardcode parameter strings as
  arrays of strings (`["0.0", "0.1", "0.2", ...]`) and convert to
  Float only for the actual render value, not the filename. See
  `scripts/docs/thin_lens_camera.rb` for the canonical pattern.

- **Memoised colour helpers.** `red`, `green`, `blue`, `yellow` in
  `lib/colors.rb` are *each* memoised in their own instance variable
  on `ElementCreator`. Calling `red()` twice returns the same texture
  instance — which is the point, it keeps the JSON small. But any
  copy-paste from one helper to another that fails to update the
  ivar name (`@red ||= ...` → `@green ||= ...` is a real example
  that shipped) silently aliases the colours. The
  `scripts/test/test_colors.rb` "different colours are distinct
  instances" test catches this; keep it green.

- **`red`/`green`/`blue`/etc. are TEXTURES, not RGB tuples.** When
  building a material in a doc-render driver, write
  `matte_material(:diffuseTexture => red)`, not
  `matte_material(:diffuseTexture => constant_color_texture(:color => red))`.
  The nested form serialises a texture into a `:color` slot, which
  is meaningless and silently produces a black material.

- **The Ruby DSL classes in `lib/scene.rb` are a parallel hierarchy
  to the C++ `Q_PROPERTY` classes.** Adding a new world class means
  manually mirroring it in `scene.rb`. Drift hazard — see
  `docs/plans/framework-critique.md` §1 for the proposed long-term
  fix (autogeneration from C++ metadata).

- **`class_doc` always produces 1 image; `property_doc` produces 5
  by default; `rainbow_doc` produces 7 (one per rainbow colour).**
  The first argument to each is the count; the keyword argument
  `aspect:` is the shape. If you need a custom count, pass it
  positionally to `property_doc(num)`.

---

## Interactive widgets

`docs/*.js` files are SVG-based interactive widgets embedded in the
Doxygen output via `@htmlonly <script src="...">`. They share a
small DOM library in `figure.js` (Vector, Canvas, Line, Circle,
Rectangle, Text, Axes, Group, Ray, DragHandler).

The standard pattern:

```js
var FooClass = new Class({
  initialize: function() {
    this.value = 0;
  },

  createCanvas: function() {
    var canvas = new Canvas(320, 240);
    canvas.translate(new Vector(...));
    canvas.add(new Axes());
    canvas.add(new Circle(..., 0.05, "result"));
    canvas.add(new Text(..., "value=" + this.value.toFixed(2)));
    return canvas.toSVG();
  }
});

(function(scriptElement) {
  var figure = new FooClass();
  var handler = new DragHandler(figure);
  handler.handlerFunc = function(delta, figure) {
    figure.value += delta.x / 100.0;
    return true;   // tells DragHandler to redraw
  };
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.scripts[document.scripts.length - 1]);
```

Embed in a header docstring with:

```cpp
* @htmlonly
* <script type="text/javascript" src="figure.js"></script>
* <script type="text/javascript" src="my_widget.js"></script>
* @endhtmlonly
```

Both are copied to `docs/html/` by `rake docs:html`.

**When to write a widget.** Widgets earn their keep when the
underlying *math* is geometrically interesting in a way that the
rendered PNG can't show — e.g. ThinLensCamera's focal-plane
convergence (the rays converge geometrically; the rendered image
just shows the consequence). Skip widgets where the rendered
output already shows the effect — the `setApertureRadius` sweep
PNGs already show DOF blur, so a widget for them would be
redundant.

See `docs/plans/framework-critique.md` §2 for known weaknesses
(2D-only primitives, single drag-horizontally interaction, no
tests).

---

## Running things

```bash
rake docs:render           # render all images (skips up-to-date)
rake check:doc-images      # lint @image html refs vs PNGs
rake test:scripts          # run Ruby tests
```

To force re-render of one driver:

```bash
ruby scripts/render_docs.rb --only thin_lens_camera --samples 64
```

To force re-render of *everything*, ignoring staleness:

```bash
rm -f docs/images/*.png.hash && rake docs:render
```

To force re-render but only to a specific PNG path:

```bash
rm docs/images/foo.png.hash && rake docs:render
```

---

## Where this is heading

`docs/plans/framework-critique.md` has the full list of proposed
improvements ranked by leverage. The four already implemented as of
this README's writing:

- ✅ Aspect-ratio overrides on `class_doc` / `property_doc`.
- ✅ Staleness detection (`.png.hash` sidecars).
- ✅ Tests for the framework (`scripts/test/`).
- ✅ Linter for `@image html` references vs on-disk PNGs.

The remaining items are deferred until they have a concrete driver:

- Ruby DSL autogeneration from C++ Q_PROPERTY metadata (single-
  consumer, not yet worth the generator).
- WebGL / three.js `Scene3D` widget primitive (no current consumer
  needs 3D in the doc widgets; ThinLens convergence works in 2D
  side-view).
- ES6 modernisation of `figure.js` (mechanical refactor, defer
  until something else makes the existing `Class()` factory hurt).
