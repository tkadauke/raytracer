#pragma once

#include <cstdint>
#include <memory>

#include "core/math/Vector.h"

namespace raytracer {
  /**
    * @brief Stream of stratified Monte-Carlo samples for a single
    *        primary-ray sample.
    *
    * The renderer's per-pixel loop produces one `SampleStream` per
    * sample-index per pixel. The camera (and, eventually, BRDFs /
    * light sources / Russian-roulette decisions in a future path
    * tracer) pulls dimensions from the stream as needed:
    *
    *     auto stream = sampler->stream(sampleIndex, pixelHash);
    *     Vector2d lensSample = stream->next2D();   // dimension 0–1
    *     double   timeSample = stream->next1D();   // dimension 2
    *
    * Each call advances an internal dimension counter, so consecutive
    * pulls return *independently stratified* samples (modulo the
    * concrete sampler's stratification scheme — see `Sampler::stream`
    * for the default implementation).
    *
    * **Why a stream and not a struct?** A struct that names dimensions
    * (`{pixelJitter, lens, time}`) hard-codes the dimensions every
    * sampler has to fill in, even ones a particular camera doesn't use.
    * A stream lets each consumer pull exactly what it needs in the
    * order it needs it — natural for a future path tracer that wants
    * arbitrary numbers of dimensions for BRDF / light sampling /
    * recursion. Same shape as PBRT's `Sampler::Get1D` / `Get2D`.
    *
    * Cameras that don't need extra dimensions can ignore the stream
    * entirely; the renderer still constructs one per sample, but the
    * default `Camera::rayForPixel(x, y)` overload doesn't read from it.
    */
  class SampleStream {
  public:
    virtual ~SampleStream() = default;

    /**
      * Pull the next 2D sample from the stream. Coordinates are
      * stratified within `[0, 1]²`. Used by:
      *
      *  - thin-lens cameras for the lens-disc sample (after a
      *    concentric square-to-disc remap)
      *  - area-light samplers for a point on the light's surface
      *  - BRDF importance sampling for a hemispherical direction
      *
      * Two consecutive calls return values from *independent*
      * dimensions. Concrete samplers may stratify these
      * independently (jittered grids), via low-discrepancy
      * sequences (Sobol, Halton), or by Owen-scrambling a base
      * sequence.
      */
    virtual Vector2d next2D() = 0;

    /**
      * Pull the next 1D sample from the stream. Stratified within
      * `[0, 1]`. Used by:
      *
      *  - motion-blur cameras for the shutter time
      *  - polarized cameras for the analyser angle
      *  - Russian-roulette decisions in a path tracer
      */
    virtual double next1D() = 0;
  };

  /**
    * @brief A `SampleStream` that always returns the centre of the
    *        unit interval / square.
    *
    * Used as the implicit default by `Camera::rayForPixel(x, y)`
    * (the no-stream convenience overload) so existing tests and
    * ad-hoc callers keep working without having to construct a
    * sampler. Returns `(0.5, 0.5)` for `next2D` and `0.5` for
    * `next1D` — for a thin-lens camera that means "lens centre,"
    * which collapses cleanly to the pinhole limit.
    *
    * Don't use this in production rendering — it's deterministic
    * (no jitter) and disabling stratification gives the camera no
    * way to do its job. Prefer `RegularSampler::stream` (or any
    * other concrete sampler).
    */
  class NullSampleStream : public SampleStream {
  public:
    Vector2d next2D() override { return Vector2d(0.5, 0.5); }
    double next1D() override { return 0.5; }
  };
}
