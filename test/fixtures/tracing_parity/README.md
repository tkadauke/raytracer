# Tracing Parity Fixtures

These scenes are the canonical fixture list for cross-backend tracing parity
tests. They are deliberately small enough for rendercli smoke tests, but each
one exercises a distinct backend contract that CPU, packed CPU, Metal, and
Vulkan intersection paths must keep aligned.

## Scene List

| Category | Scene | Contract |
| --- | --- | --- |
| Matte direct light | `matte_direct_light.json` | Opaque analytic primitives with a deterministic direct-light path. |
| Indirect bounce | `indirect_bounce.json` | Path-traced diffuse bounce from a colored wall onto matte receivers. |
| Imported mesh | `imported_mesh.json` | Source-backed STL import through `SourceAsset`, producing mesh triangles instead of analytic primitives. |
| Transparent fallback | `transparent_fallback.json` | Transparent material leaves that are expected to remain on the runtime CPU fallback path until recursive material metadata is supported by packed/GPU backends. |
| Visibility-heavy | `visibility_heavy.json` | Area-light direct sampling with many shadow blockers, stressing any-hit visibility batches. |

`manifest.json` repeats the same list in machine-readable form for future test
helpers. Tests should prefer the manifest category names over inventing new
ad-hoc fixture paths.

## Suggested Rendercli Shape

Parity tests should render these scenes with explicit image dimensions,
sampling seed, sample count, recursion depth, and backend request. Keep the CPU
render as the reference and compare GPU or packed-CPU candidates with a small
RMS tolerance rather than exact PNG bytes unless the test has already proven the
output is deterministic across platforms.

Example:

```sh
rendercli --engine wavefront --integrator pathtracer \
  --wavefront_intersection_backend cpu \
  --width 32 --height 24 --samples_per_pixel 4 --sampling_seed 1337 \
  test/fixtures/tracing_parity/matte_direct_light.json out.png
```
