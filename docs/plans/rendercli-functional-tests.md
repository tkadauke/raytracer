# Rendercli functional test coverage plan - May 2026

> **Scope:** make `rendercli`'s CTest coverage systematic. The target is not
> pixel-perfect regression coverage for every renderer algorithm; it is command
> behavior coverage for every public CLI path: parsing, validation, rendering
> artifacts, stdout/stderr contracts, graph artifacts, animation output, and
> cross-option rejection rules.

## Current state

`rendercli` has two CMake-script tests registered from `test/CMakeLists.txt`:

- `rendercli_animation` runs `test/rendercli/FrameOptionTest.cmake`.
- `rendercli_render_graph` runs `test/rendercli/RenderGraphOptionTest.cmake`.

Those tests cover important new paths: `--frame`, `--animation`,
`--render_graph_only`, graph text/DOT/JSON export, graph JSON replay, graph
disable filters, graph intent overrides, the default graph render path, and the
`--direct_engine` bypass. They do not yet cover the full base renderer command
surface, most raster-specific flags, all parse failures, or output-image
invariants beyond "file exists" and a few "frames differ" checks.

## Principles

- Keep these as functional tests that invoke the real `rendercli` binary through
  CTest. Unit tests can pin parser helper functions later if the parser is
  extracted, but the high-value regression surface is the installed command.
- Keep each test script focused by feature family. One giant script becomes too
  hard to diagnose when a failure happens.
- Prefer tiny scenes and tiny output images. Most command behavior can be tested
  at 16-64 pixels.
- Avoid fragile golden full-image comparisons for complex raytraced content.
  Use dimensions, file existence, non-empty output, stdout/stderr regexes,
  graph JSON/DOT content, and "different from baseline" checks first.
- Use image comparison only where the command is specifically meant to change
  pixels and the scene is deterministic.
- Exercise both success and failure paths. Parse validation is part of the
  public interface.

## Harness improvements

### Add shared CMake helpers

Create `test/rendercli/RendercliTestHelpers.cmake` with:

- `run_rendercli(name ...args...)` returning stdout, stderr, result, and a
  helpful failure message;
- `expect_success(name ...args...)`;
- `expect_failure(name expected_stderr_regex ...args...)`;
- `expect_file_exists(path)`;
- `expect_file_absent(path)`;
- `expect_file_nonempty(path)`;
- `expect_files_differ(a b)`;
- `expect_stdout_matches(name regex)`;
- `expect_stderr_matches(name regex)`.

Then update `FrameOptionTest.cmake` and `RenderGraphOptionTest.cmake` to use
the shared helpers. This should be the first implementation step so future
coverage stays readable.

### Add a small image probe executable

CMake alone is weak at image assertions. Add a tiny Qt-backed test utility,
for example `test/rendercli/rendercli_image_probe.cpp`, that can print:

```text
width=64 height=32 nonzero_pixels=1234 hash=...
```

Use it from CMake scripts to assert:

- output dimensions match requested dimensions or graph replay dimensions;
- images are not empty;
- two renders differ when a feature should visibly affect output;
- two renders match when a deterministic command path should be equivalent.

The hash should be stable over raw decoded pixels, not over PNG bytes.

### Register one CTest per feature family

Keep test names narrow:

- `rendercli_basic`
- `rendercli_validation`
- `rendercli_raytracer`
- `rendercli_wireframe`
- `rendercli_raster`
- `rendercli_raster_output_state`
- `rendercli_raster_shadows`
- `rendercli_animation`
- `rendercli_render_graph`

CTest `-R rendercli` should run all command tests, while `-R
rendercli_raster` should let us focus on raster options quickly.

## Coverage matrix

### Basic command behavior

Test file: `test/rendercli/BasicOptionTest.cmake`

Success cases:

- default render with only input/output uses raytracer and creates a non-empty
  image;
- `--engine raytracer`, `--engine raster`, and `--engine wireframe` each render
  a tiny image;
- `--width` and `--height` control output dimensions;
- `--tonemap Linear`, `Reinhard`, and `ACES` are accepted;
- `--timing` prints a `render_ms` line;
- `--repeat 2` renders successfully and prints timing;
- `--help` exits successfully and prints options;
- `--version` exits successfully.

Failure cases:

- missing input/output;
- missing output outside `--render_graph_only`;
- unreadable scene file;
- unwritable output path;
- invalid engine;
- invalid width/height;
- invalid recursion depth;
- invalid samples-per-pixel;
- invalid thread count;
- invalid queue size;
- invalid repeat count.

### Raytracer options

Test file: `test/rendercli/RaytracerOptionTest.cmake`

Success cases:

- `--depth 1` and a higher `--depth` both render reflective/transmissive
  scenes;
- `--sampler Regular`, `Random`, and `Jittered` are accepted;
- `--samples_per_pixel` affects sampler setup and still writes the requested
  dimensions;
- `--threads` and `--queue_size` render successfully.

Failure cases:

- unknown sampler type should be handled deliberately. If the current behavior
  crashes or dereferences null, fix the implementation and then pin the error.

Pixel checks:

- use a deterministic tiny reflective scene and compare `--depth 1` to a deeper
  render only if the difference is stable.

### Wireframe options

Test file: `test/rendercli/WireframeOptionTest.cmake`

Success cases:

- `--engine wireframe --lod 0`, `--lod 1`, and a higher LOD render;
- output dimensions are correct;
- increasing LOD changes the pixel hash for a curved primitive scene.

Failure cases:

- non-integer `--lod`;
- negative `--lod`.

### Raster base options

Test file: `test/rendercli/RasterOptionTest.cmake`

Success cases:

- `--engine raster` renders a basic scene;
- `--lod` changes curved primitive output;
- `--cull both|back|front` are accepted;
- `--msaa 1|2|4|8` are accepted and preserve output dimensions;
- `--msaa_shading per_sample|per_fragment` are accepted;
- `--post_aa none|fxaa|smaa|taa` are accepted;
- recursive-material fallback warnings appear for reflective/transmissive
  scenes and not for simple matte scenes.

Failure cases:

- invalid `--cull`;
- invalid `--msaa`;
- invalid `--msaa_shading`;
- invalid `--post_aa`.

Pixel checks:

- compare `--post_aa none` to `fxaa` or `smaa` on a high-contrast edge scene;
- compare raster fallback scene stderr to expected warnings.

### Raster color-output and viewport state

Test file: `test/rendercli/RasterOutputStateOptionTest.cmake`

Success cases:

- `--color_write_mask rgb`, single-channel masks, combined masks, `none`;
- `--blend` with representative source/destination factors;
- `--blend_op add|subtract|reverse_subtract|min|max`;
- `--blend_constant_color` and `--blend_constant_alpha`;
- `--alpha_test`, `--alpha_func`, and `--alpha_ref`;
- `--viewport` and `--scissor`;
- `--depth_bias`.

Failure cases:

- invalid color write mask;
- invalid blend factor;
- invalid blend op;
- invalid blend constant color;
- invalid blend constant alpha;
- invalid alpha function;
- invalid alpha reference;
- malformed viewport/scissor rectangles;
- non-finite or unparsable depth bias.

Pixel checks:

- use tiny scenes tailored for each state group and compare against baseline
  hashes or "different from baseline" assertions. The docs-render scenes for
  color output are good candidates if they can be reused directly.

### Raster shadow options

Test file: `test/rendercli/RasterShadowOptionTest.cmake`

Success cases:

- `--shadow_maps`;
- representative `--shadow_map_size`;
- `--shadow_cascades 1`, `2`, and `4`;
- representative `--shadow_cascade_split`;
- representative `--shadow_bias`;
- representative `--shadow_slope_bias`;
- representative `--shadow_filter_radius`;
- `--shadow_filter pcf|pcss`.

Failure cases:

- non-positive shadow map size;
- non-positive cascade count;
- split outside `[0,1]`;
- negative bias/slope bias;
- negative filter radius;
- invalid filter mode.

Pixel checks:

- use a deterministic shadow scene and assert `--shadow_maps` differs from no
  shadows. Avoid over-testing exact bias/filter output unless the scene is
  intentionally built to make the difference stable.

### Animation options

Existing file: `test/rendercli/FrameOptionTest.cmake`

Keep current coverage, then add:

- invalid `--frame_start`;
- invalid `--frame_end`;
- invalid `--fps`;
- `--animation` combined with `--frame`;
- `--animation` combined with `--repeat`;
- `--animation` combined with graph-only or graph-output modes;
- `%d` and `%04i` accepted placeholders;
- multiple placeholders rejected;
- incomplete placeholder rejected;
- output directory missing behavior is pinned, either accepted if Qt creates it
  indirectly or rejected with a clear error.

### Render graph options

Existing file: `test/rendercli/RenderGraphOptionTest.cmake`

Keep current coverage, then add:

- invalid `--render_graph_format`;
- invalid `--render_graph_executor`;
- invalid `--render_graph_view`;
- invalid `--disable_pass_kind`;
- invalid `--disable_executor`;
- comma-separated and repeated disable filters in one command;
- `--render_graph_out` while rendering writes both image and graph;
- `--render_graph_only` with no output writes graph to stdout;
- malformed JSON in `--render_graph_in`;
- JSON root not an object;
- valid JSON plan with semantic validation failure reports graph validation;
- `--direct_engine` combined with graph controls is rejected; ✅ covered for
  graph-only export in `RenderGraphOptionTest.cmake`.
- `--render_graph_only` combined with `--repeat` is rejected;
- `--render_graph_only` combined with animation is rejected;
- graph replay with matching explicit width/height succeeds;
- graph replay with mismatched explicit width/height fails.

### Cross-option validation

Test file: `test/rendercli/CrossOptionValidationTest.cmake`

Coverage:

- static scene with `--frame` succeeds;
- static scene with `--animation` fails;
- `--animation --frame` fails;
- `--animation --repeat` fails;
- `--animation --render_graph_only` fails;
- `--animation --render_graph_out` fails;
- raster-only flags passed with non-raster engines either remain accepted and
  inert or become rejected. Pick the intended policy and pin it.

## Fixtures

Prefer existing scene files when possible:

- `scenes/dice.json` - generic static scene;
- `scenes/animation_frame_demo.json` - animation frame differences;
- `scenes/animated_*.json` - animation fixture loadability;
- `scenes/raster_material_preview.json` - raster material checks;
- `scenes/reflections.json` or a smaller new reflective fixture - recursion
  depth and raster fallback warnings.

Add small dedicated fixtures only when existing scenes make the assertion
fragile:

- high-contrast edge scene for post-AA;
- simple shadow receiver/caster scene for shadow toggles;
- color-output state scene with known source/destination colors;
- tiny wireframe curved primitive scene for LOD changes.

Keep rendercli fixtures under `test/fixtures/rendercli/` if they are test-only.
Use `scenes/` only for reusable demos.

## Implementation order

1. Add shared CMake helper functions and migrate existing rendercli tests.
2. Add the image probe utility and basic image assertions.
3. Add `rendercli_basic` and `rendercli_validation`; these catch the largest
   user-facing breakage surface.
4. Expand animation and render-graph negative coverage.
5. Add wireframe and raytracer option tests.
6. Add raster base option tests.
7. Add raster output-state tests.
8. Add raster shadow tests.
9. Add cross-option policy tests.
10. Add a maintenance check that every `parser.addOptions(...)` long option in
    `tools/rendercli/rendercli.cpp` is mentioned in this plan or a rendercli
    CMake test. This can start as a script that reports missing options and
    later become a CTest failure.

## Done criteria

- `ctest --preset release -R rendercli --output-on-failure` covers every
  documented rendercli option at least once.
- Every option with custom parser validation has at least one invalid-value
  test.
- Every option that writes a different artifact type has an artifact assertion:
  image file, graph file, stdout graph, animation frame sequence, or stderr
  diagnostic.
- Every cross-option rejection in `Renderer::parseCommandLine` has a test.
- The rendercli functional tests run fast enough to stay in the default release
  CTest preset.
- Adding a new rendercli option requires updating either a rendercli CMake test
  or the explicit coverage allowlist.
