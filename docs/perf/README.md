# Performance Evidence

This directory stores benchmark captures and decision notes that support
performance-sensitive changes. Prefer adding a short Markdown note with the
hardware, compiler, command line, active `RAYTRACER_SIMD_*` gates, and relevant
before/after numbers instead of leaving benchmark evidence only in a PR body.

## ARM SIMD

Epic #426 ARM SIMD evidence is split by decision point:

- `arm-simd-phase0-baseline-2026-05-28.md` — pre-NEON Apple Silicon math
  baseline plus x86 packet baseline.
- `arm-simd-phase2-packet-kernels-2026-05-29.md` — shared `Float4`/`Mask4`
  Ray4 packet-kernel evidence after the SSE and NEON-capable backend landed.
- `arm-simd-phase3-float-math-candidates-2026-05-30.md` — float
  math/color candidate decision and future ARM rerun filter.
- `arm-simd-phase4-double-precision-candidates-2026-05-30.md` — double
  precision candidate decision; ARM keeps the generic/autovectorized path until
  a hot benchmark proves a NEON win.
- `arm-simd-phase5-ray8-policy-2026-05-30.md` — Ray8 policy; ARM stays centered
  on Ray4 because NEON is 128-bit, while Ray8 remains AVX-only.

For new SIMD work, keep x86-specific terms precise: SSE/SSE2/SSE3/AVX are x86
features, NEON is the ARM feature, and shared packet algorithms should usually
be described as SIMD.
