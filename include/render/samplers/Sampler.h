#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

#include "core/math/Vector.h"
#include "core/math/Number.h"
#include "render/samplers/SampleStream.h"

namespace render {
  class Sampler;

  /**
    * @brief Default sample stream backed by a `Sampler`'s pre-baked sets.
    *
    * This is the concrete implementation returned by `Sampler::stream()` and
    * `Sampler::sharedStream()` for ordinary samplers. Wavefront can also store
    * these by value for a tile so retained primary-ray sample streams do not
    * require one heap allocation per sample.
    */
  class SamplerSampleStream : public SampleStream {
  public:
    SamplerSampleStream(const Sampler& sampler, int sampleIndex, std::uint64_t pixelHash);

    Vector2d next2D() override;
    double next1D() override;
    PrimarySample primarySample() override;
    Vector2d sample2D(SampleDimension dimension, std::uint64_t index = 0) override;
    double sample1D(SampleDimension dimension, std::uint64_t index = 0) override;

  private:
    Vector2d sampleForDimension(std::uint64_t dimension) const;

    const Sampler* m_sampler;
    int m_sampleIndex;
    std::uint64_t m_pixelHash;
    std::uint64_t m_dim;
  };

  class SampleStreamStorage {
  public:
    void reserve(std::size_t count);
    SampleStream* appendOwned(std::shared_ptr<SampleStream> stream);
    SampleStream* appendSamplerBacked(const Sampler& sampler, int sampleIndex,
                                      std::uint64_t pixelHash);

  private:
    std::vector<std::shared_ptr<SampleStream>> m_ownedStreams;
    std::vector<SamplerSampleStream> m_samplerBackedStreams;
    std::deque<SamplerSampleStream> m_samplerBackedOverflowStreams;
  };

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
    *     Vector2d bsdfSample =
    *       stream->sample2D(SampleDimension::BSDF, bounce);
    *
    * The stream API is what cameras with extra stochastic dimensions
    * (thin-lens for lens samples, motion-blur cameras for shutter
    * time, future polarized cameras for analyser angle) use. Same
    * shape as PBRT's `Sampler::Get1D` / `Get2D`.
    *
    * The default `stream` implementation reads from the pre-baked
    * sample sets, indexing each requested dimension at
    * `(pixelHash + dim) mod numSets` so consecutive calls and explicit
    * named dimensions return stratified samples from *independent* sets
    * — that's what decorrelates dimensions across a path. Concrete samplers that
    * implement low-discrepancy sequences (Sobol, Halton, future
    * QMC samplers) will override `stream` to return a sequence-aware
    * stream rather than the default per-set look-up.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="sampler_streams.js"></script>
    * @endhtmlonly
    */
  class Sampler {
  public:
    inline Sampler()
        : m_numSamples(0),
          m_numSets(0) {
    }
    inline virtual ~Sampler() {
    }

    void setup(int numSamples, int numSets);
    void setup(int numSamples, int numSets, std::uint64_t seed);

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
      * Returns one sample from the stream-owned dimension lookup. The default
      * uses the pre-generated set `(pixelHash + dimension) mod numSets` at
      * `sampleIndex`. Samplers can override this when a dimension needs
      * sequence-aware scrambling while preserving the public `SampleStream`
      * contract.
      */
    virtual Vector2d sampleForDimension(int sampleIndex, std::uint64_t pixelHash,
                                        std::uint64_t dimension) const;

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
      * The default implementation returns a stream that reads `next2D`
      * from set `(pixelHash + dim) mod numSets` at sampleIndex, and
      * `next1D` from the x-coordinate of the same lookup. Explicit
      * `SampleDimension` requests use the same mapping with stable
      * renderer/path-tracing dimension ownership. Concrete samplers may
      * override this to produce sequence-aware streams (Sobol, Halton,
      * …); see SampleStream.h for the consumer-side contract.
      */
    virtual std::unique_ptr<SampleStream> stream(int sampleIndex, uint64_t pixelHash) const;

    /**
      * Returns a retained `SampleStream` for render paths that need to keep
      * streams alive after primary-ray generation.
      *
      * Wavefront batch renderers generate every primary sample first and then
      * hand the retained streams to an integrator. This method preserves the
      * same sample sequence as `stream(...)`; built-in sampler subclasses
      * override it to allocate the stream object and shared control block
      * together.
      */
    virtual std::shared_ptr<SampleStream> sharedStream(int sampleIndex, uint64_t pixelHash) const;

    /**
      * Appends a retained stream to caller-owned storage and returns a
      * non-owning pointer valid for the storage lifetime.
      *
      * Built-in samplers use this to store their default stream by value in
      * wavefront tile batches. Custom sampler subclasses can keep overriding
      * `stream(...)`; the base implementation stores the resulting stream in
      * owning storage.
      */
    virtual SampleStream* appendStream(SampleStreamStorage& storage, int sampleIndex,
                                       uint64_t pixelHash) const;

  protected:
    std::shared_ptr<SampleStream> sharedSamplerBackedStream(int sampleIndex,
                                                            uint64_t pixelHash) const;
    SampleStream* appendSamplerBackedStream(SampleStreamStorage& storage, int sampleIndex,
                                            uint64_t pixelHash) const;
    bool isPathTracingDimension(uint64_t dimension) const;
    Vector2d offsetScrambledPathDimensionSample(int sampleIndex, uint64_t pixelHash,
                                                uint64_t dimension) const;
    double scrambledOffset(int sampleIndex, uint64_t pixelHash, uint64_t dimension,
                           uint64_t axis) const;
    double wrapUnitInterval(double value) const;

    virtual std::vector<Vector2d> generateSet() = 0;

  private:
    std::vector<std::vector<Vector2d>> m_sampleSets;
    int m_numSamples;
    int m_numSets;
  };
}
