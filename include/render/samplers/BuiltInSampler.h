#pragma once

#include <cstdint>
#include <memory>

#include "render/samplers/Sampler.h"

namespace render {
  /**
    * @brief Shared plumbing for the library's built-in samplers (Halton,
    *        Jittered, Random, Regular).
    *
    * Retained sample streams are backed directly by the sampler's pre-baked
    * sets via `sharedSamplerBackedStream`/`appendSamplerBackedStream`,
    * avoiding the extra heap allocation the base `Sampler::sharedStream`/
    * `appendStream` defaults incur. Custom sampler subclasses outside this
    * library can keep overriding the base `Sampler` methods directly.
    */
  class BuiltInSampler : public Sampler {
  public:
    std::shared_ptr<SampleStream> sharedStream(int sampleIndex, uint64_t pixelHash) const override;
    SampleStream* appendStream(SampleStreamStorage& storage, int sampleIndex,
                               uint64_t pixelHash) const override;

  protected:
    /**
      * Returns the default per-set sample for non-path-tracing dimensions,
      * or a scrambled, jitter-offset sample for path-tracing dimensions
      * (BSDF, light, continuation, ...). Used by `GridSampleAwareSampler`,
      * the shared base for the grid-based built-in samplers
      * (`JitteredSampler`, `RegularSampler`).
      */
    Vector2d scrambledPathAwareSample(int sampleIndex, uint64_t pixelHash,
                                      uint64_t dimension) const;
  };
}
