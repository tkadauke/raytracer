# Tracing execution backends

> **Scope:** backend-neutral contracts for CPU and future GPU tracing paths.
> This plan records decisions that must be shared by scalar CPU, wavefront CPU,
> and GPU-assisted tracing so parity failures do not hide behind scheduler or
> platform differences.

## GPU sample stream

The first GPU tracing sample generator is a stateless 32-bit PCG hash
(`pcg_hash32`) evaluated from an explicit sample coordinate. The generator is a
deterministic building block, not a new renderer:

```cpp
uint32_t pcg_hash32(uint32_t input) {
  uint32_t state = input * 747796405u + 2891336453u;
  uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}
```

Each stochastic request is addressed by the same logical coordinate already
reserved by `SampleStream`: `(seed, pixelIndex, primarySampleIndex, dimension,
component)`. The dimension comes from `sampleDimensionIndex(name, slot)`.
Pixel, time, and lens own dimensions 0, 1, and 2; path-tracing dimensions repeat
in four-slot groups for BSDF, light-surface, direct-light selection, and
Russian-roulette continuation. The coordinate is folded through fixed unsigned
32-bit integer mixes before the final `pcg_hash32` call; future fixed-vector
tests must pin the exact packing/mixing constants before production code
depends on this stream.

Floating-point samples are produced by taking the high 24 bits of the hash and
multiplying by `2^-24`, yielding a value in `[0, 1)`. That rule is intentional:
it avoids platform `uniform_real_distribution` behavior, avoids CPU extended
precision differences, and maps cleanly to `float` precision on GPU shaders.
The CPU reference implementation lives in `render::GpuSampleStream` and is
kept independent of the existing stratified sampler set path.

### Rationale

- **Schedule-independent.** A sample is a pure function of its coordinate, so
  scalar CPU, wavefront CPU, and GPU queues can request dimensions in different
  orders without changing the stream.
- **GPU-friendly.** The hash uses only unsigned 32-bit multiply, add, xor, and
  shift operations. Those are widely available in C++, GLSL, MSL, WGSL, and
  SPIR-V without relying on 64-bit integer support.
- **No hidden RNG state.** The contract forbids `std::random_device`,
  `std::mt19937`, `std::uniform_real_distribution`, Qt random helpers,
  shader `fract(sin(...))` tricks, hardware RNGs, or any per-thread/per-platform
  generator for this stream.
- **Easy parity testing.** Fixed input coordinates produce fixed `uint32_t` and
  `[0, 1)` values that can be checked in ordinary unit tests and mirrored in
  shader conformance tests.

### Limitations

- This is a pseudorandom white-noise baseline, not a low-discrepancy sequence.
  Sobol, Owen-scrambled Sobol, blue-noise tiles, and stratified reconstruction
  remain future sampling-quality work.
- It is not cryptographic and must not be used for secrets, randomized file
  formats, or adversarial input handling.
- The initial 24-bit float mapping is sufficient for renderer sampling parity,
  but not for APIs that require full 32-bit integer entropy as a float.
- The exact coordinate packing is part of the ABI once fixed-vector tests land;
  changing it is a behavior change and must be versioned or intentionally
  migrated.
