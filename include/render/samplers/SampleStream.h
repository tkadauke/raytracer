#pragma once

#include <cstdint>
#include <memory>

#include "core/math/Vector.h"

namespace render {
  /**
    * Named stochastic dimensions reserved by the renderer and future
    * integrators.
    *
    * Pixel, time, and lens keep the existing renderer/camera stream order:
    * the render loop consumes pixel jitter first, shutter time second, and
    * thin-lens cameras consume aperture samples after that. Path-tracing
    * dimensions are indexed by bounce/sample depth so BSDF, light, and
    * continuation requests cannot accidentally reuse the same 2D pattern.
    * Light selection uses `SampleDimension::LightSelection`; selected-light
    * surface samples include the light index through
    * `SampleStream::lightSampleIndex(...)` so multiple stochastic lights at one
    * bounce do not share a surface sample.
    */
  enum class SampleDimension { Pixel, Time, Lens, BSDF, Light, LightSelection, Continuation };

  /**
    * Maps a named dimension and optional depth/index to the stream's stable
    * numeric dimension.
    */
  constexpr std::uint64_t sampleDimensionIndex(SampleDimension dimension, std::uint64_t index = 0) {
    switch (dimension) {
    case SampleDimension::Pixel:
      return 0;
    case SampleDimension::Time:
      return 1;
    case SampleDimension::Lens:
      return 2;
    case SampleDimension::BSDF:
      return 3 + index * 3;
    case SampleDimension::Light:
      return 4 + index * 3;
    case SampleDimension::LightSelection:
      return 1000019ull + index;
    case SampleDimension::Continuation:
      return 5 + index * 3;
    }
    return 0;
  }

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
    *     Vector2d lensSample = stream->next2D();
    *     double   timeSample = stream->next1D();
    *     Vector2d bsdfSample =
    *       stream->sample2D(SampleDimension::BSDF, bounce);
    *
    * Each call advances an internal dimension counter, so consecutive
    * pulls return *independently stratified* samples (modulo the
    * concrete sampler's stratification scheme). See `Sampler::stream`
    * for the default implementation and the interactive sampler-stream
    * widget that shows why pixel, lens, and shutter-time dimensions
    * should not reuse the same 2D pattern.
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
    struct PrimarySample {
      Vector2d pixel;
      double time{0.0};
    };

    /**
      * Returns the stable `SampleDimension::Light` index for one light sample
      * at one path bounce.
      *
      * The first light at bounce 0 intentionally maps to index 0 so existing
      * callers/tests that use `sample2D(SampleDimension::Light, 0)` keep the
      * same sample. Additional lights and later bounces use Cantor pairing so
      * every `(bounce, lightIndex)` pair owns a distinct light-sampling
      * dimension without colliding with BSDF or continuation dimensions.
      */
    static constexpr std::uint64_t directLightSampleIndexStride() {
      return 1048576ull;
    }

    static constexpr std::uint64_t lightSelectionSampleIndex(std::uint64_t bounce,
                                                             std::uint64_t directSampleIndex = 0) {
      return bounce + directSampleIndex * directLightSampleIndexStride();
    }

    static constexpr std::uint64_t lightSampleIndex(std::uint64_t bounce, std::uint64_t lightIndex,
                                                    std::uint64_t directSampleIndex = 0) {
      const std::uint64_t effectiveBounce = lightSelectionSampleIndex(bounce, directSampleIndex);
      const std::uint64_t sum = effectiveBounce + lightIndex;
      return sum * (sum + 1u) / 2u + lightIndex;
    }

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

    /**
      * Pull the renderer-owned primary-ray dimensions as one operation:
      * sub-pixel jitter followed by shutter time. The default preserves the
      * legacy sequential cursor behavior; sampler-backed streams can override
      * it to avoid multiple virtual dispatches in wavefront batch setup.
      */
    virtual PrimarySample primarySample() {
      return PrimarySample{next2D(), next1D()};
    }

    /**
      * Pull a 2D sample from a named dimension without advancing the
      * sequential `next*` cursor. Integrators should prefer this for
      * path-tracing dimensions so BSDF, light, and continuation samples
      * have deterministic ownership independent of call order.
      */
    virtual Vector2d sample2D(SampleDimension dimension, std::uint64_t index = 0) = 0;

    /**
      * Pull a 1D sample from a named dimension without advancing the
      * sequential `next*` cursor.
      */
    virtual double sample1D(SampleDimension dimension, std::uint64_t index = 0) = 0;
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
    Vector2d next2D() override {
      return Vector2d(0.5, 0.5);
    }
    double next1D() override {
      return 0.5;
    }
    Vector2d sample2D(SampleDimension, std::uint64_t = 0) override {
      return Vector2d(0.5, 0.5);
    }
    double sample1D(SampleDimension, std::uint64_t = 0) override {
      return 0.5;
    }
  };
}
