# Raster Baseline Scenes - 2026-05-31

Epic #356 needs repeatable raster captures before the next optimization work.
Use the wrapper below so the scene, resolution, MSAA, LOD, timing, metrics JSON,
and optional counter AOVs stay consistent across jobs.

## Commands

Build rendercli once:

```sh
cmake --preset release
cmake --build --preset release --target rendercli
```

Run one baseline scene:

```sh
benchmarks/raster_baseline_capture.sh materials
benchmarks/raster_baseline_capture.sh dense_sphere
benchmarks/raster_baseline_capture.sh offscreen_floor
benchmarks/raster_baseline_capture.sh alpha_blend_stencil
benchmarks/raster_baseline_capture.sh dense_ldraw
```

Run the full baseline set:

```sh
benchmarks/raster_baseline_capture.sh all
```

Add the five raster counter AOV preview PNGs:

```sh
benchmarks/raster_baseline_capture.sh --aovs all
```

Outputs default to `tmp/raster-baselines/<scene>/`. Override the rendercli path
or output directory with `RENDERCLI=/path/to/rendercli` and
`RASTER_BASELINE_OUT=/path/to/output`.

Use the command without `--aovs` for timing baselines. The AOV mode executes and
exports extra graph views, so its wall-clock totals are visual-reference
captures rather than comparable performance numbers.

## Coverage

The harness captures every scene at 640x480, MSAA 1 and 4, LOD 0 and LOD 2,
with `--repeat 5`, `--raster_metrics_out`, and `--raster_metrics_summary`.
LOD 0 is the current raster default and minimum; a lower-than-default capture is
therefore not meaningful until a future screen-space LOD quality control exists.
LOD 2 is the higher-detail comparison point.

The dense import case uses a generated MPD under the output directory instead
of `~/Downloads/10018-1.mpd`. The fixture is deterministic, project-owned, and
made from repeated curved submodels so it exercises dense imported curved
geometry without redistributing third-party LDraw model data.

`alpha_blend_stencil` is the fixed-function safety scene. The wrapper enables
source-alpha blending, alpha testing, and the stencil-composite graph view for
that scene, so later optimizations have a baseline that is sensitive to
order-dependent raster state.

## Initial Baseline Numbers

Initial numbers are from the project release build in the Syrus Linux runner on
2026-05-31. They are reference starting points, not pass/fail thresholds.

| Scene | MSAA | LOD | Median render_ms | Notes |
| --- | ---: | ---: | ---: | --- |
| materials | 1 | 0 | 93.162 ms | Capture with `benchmarks/raster_baseline_capture.sh materials`. |
| materials | 4 | 0 | 200.990 ms | Same scene, 4x MSAA. |
| materials | 1 | 2 | 108.893 ms | Higher LOD comparison. |
| materials | 4 | 2 | 230.718 ms | Higher LOD plus 4x MSAA. |
| dense_sphere | 1 | 0 | 46.005 ms | Capture with `benchmarks/raster_baseline_capture.sh dense_sphere`. |
| dense_sphere | 4 | 0 | 53.609 ms | Same scene, 4x MSAA. |
| dense_sphere | 1 | 2 | 54.309 ms | Higher LOD comparison. |
| dense_sphere | 4 | 2 | 78.650 ms | Higher LOD plus 4x MSAA. |
| offscreen_floor | 1 | 0 | 103.364 ms | Capture with `benchmarks/raster_baseline_capture.sh offscreen_floor`. |
| offscreen_floor | 4 | 0 | 267.782 ms | Same scene, 4x MSAA. |
| offscreen_floor | 1 | 2 | 109.285 ms | Higher LOD comparison. |
| offscreen_floor | 4 | 2 | 244.201 ms | Higher LOD plus 4x MSAA. |
| alpha_blend_stencil | 1 | 0 | 150.194 ms | Fixed-function safety scene. |
| alpha_blend_stencil | 4 | 0 | 206.652 ms | Fixed-function safety scene with 4x MSAA. |
| alpha_blend_stencil | 1 | 2 | 159.592 ms | Higher LOD comparison. |
| alpha_blend_stencil | 4 | 2 | 251.968 ms | Higher LOD plus 4x MSAA. |
| dense_ldraw | 1 | 0 | 51.503 ms | Generated dense curved import fixture. |
| dense_ldraw | 4 | 0 | 61.440 ms | Generated dense curved import fixture with 4x MSAA. |
| dense_ldraw | 1 | 2 | 54.040 ms | Import geometry is fixed, but this keeps the command matrix consistent. |
| dense_ldraw | 4 | 2 | 60.723 ms | Import geometry is fixed, plus 4x MSAA. |
