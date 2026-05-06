#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "core/math/Vector.h"
#include "core/math/Number.h"
#include "raytracer/samplers/SampleStream.h"

namespace raytracer {
  /**
    * @brief Stratified Monte-Carlo sampler — produces pre-baked sets of
    *        2D samples in `[0, 1]²` and exposes them through two
    *        complementary access patterns.
    *
    * **The set-based API (`sampleSet`).** Returns a flat
    * `std::vector<Vector2d>` of `numSamples` 2D points, one of
    * `numSets` pre-generated sets chosen at random. This is the
    * legacy interface — `Camera::render` walks one set per pixel and
    * passes each `Vector2d` to `rayForPixel(pixel + sample)`. Cameras
    * that only need a single 2D dimension per sample (pinhole,
    * orthographic, fisheye, spherical, equirectangular — anything
    * where the sub-pixel offset is the only stochastic input) work
    * fine with this API.
    *
    * **The stream-based API (`stream`).** Returns a `SampleStream`
    * that the camera (or, eventually, BRDFs / light sources / a path
    * tracer) pulls dimensions from on demand:
    *
    *     auto stream = sampler->stream(sampleIndex, pixelHash);
    *     Vector2d lensSample = stream->next2D();
    *
    * The stream API is what cameras with extra stochastic dimensions
    * (thin-lens for lens samples, motion-blur cameras for shutter
    * time, future polarized cameras for analyser angle) use. Same
    * shape as PBRT's `Sampler::Get1D` / `Get2D`.
    *
    * The default `stream` implementation reads from the pre-baked
    * sample sets, indexing each requested dimension at
    * `(pixelHash + dim) mod numSets` so consecutive calls return
    * stratified samples from *independent* sets — that's what
    * decorrelates dimensions across a path. Concrete samplers that
    * implement low-discrepancy sequences (Sobol, Halton, future
    * QMC samplers) will override `stream` to return a sequence-aware
    * stream rather than the default per-set look-up.
    */
  class Sampler {
  public:
    inline Sampler()
      : m_numSamples(0),
        m_numSets(0)
    {
    }
    inline virtual ~Sampler() {}

    void setup(int numSamples, int numSets);

    inline int numSamples() const {
      return m_numSamples;
    }

    inline int numSets() const {
      return m_numSets;
    }

    /**
      * Picks a random set and returns it. Used by the legacy
      * single-dimension `Camera::render` path that walks one full
      * set per pixel.
      */
    inline const std::vector<Vector2d>& sampleSet() const {
      return m_sampleSets[random(m_numSets)];
    }

    /**
      * Returns the i-th pre-baked sample set. The default `stream`
      * implementation uses this to look up successive dimensions; not
      * intended for general callers.
      */
    inline const std::vector<Vector2d>& setAt(int i) const {
      return m_sampleSets[i];
    }

    /**
      * Returns a `SampleStream` for the given primary-ray sample at a
      * given pixel.
      *
      * @param sampleIndex which of the `numSamples` per-pixel samples
      *        we're producing (0 ≤ sampleIndex < numSamples).
      * @param pixelHash a hash of the pixel coordinates (or anything
      *        else that varies per pixel) used to decorrelate the
      *        dimension lookups across pixels — without this every
      *        pixel would draw the same sequence of pre-baked sets
      *        and you'd see structured noise patterns aligned to the
      *        pixel grid.
      *
      * The default implementation returns a stream that reads
      * `next2D` from set `(pixelHash + dim) mod numSets` at
      * sampleIndex, and `next1D` from the x-coordinate of the same
      * lookup. Concrete samplers may override this to produce
      * sequence-aware streams (Sobol, Halton, …); see SampleStream.h
      * for the consumer-side contract.
      */
    virtual std::unique_ptr<SampleStream> stream(int sampleIndex, uint64_t pixelHash) const;

  protected:
    virtual std::vector<Vector2d> generateSet() = 0;

  private:
    std::vector<std::vector<Vector2d>> m_sampleSets;
    int m_numSamples;
    int m_numSets;
  };
}
