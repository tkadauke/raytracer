#pragma once
#include <vector>

#include "render/samplers/Sampler.h"

namespace render {
  /**
    * Samples the pixels in a regular grid, but each ray is displaced by a
    * random amount within its own cell. The grid still gives each stratum
    * one sample; see `Sampler` for the interactive sampler-stream widget
    * that compares regular, jittered, and random sample dimensions.
    * 
    * <table><tr>
    * <td>@image html jittered_sampler_spp_1.png "samplesPerPixel=1"</td>
    * <td>@image html jittered_sampler_spp_4.png "samplesPerPixel=4"</td>
    * <td>@image html jittered_sampler_spp_9.png "samplesPerPixel=9"</td>
    * <td>@image html jittered_sampler_spp_16.png "samplesPerPixel=16"</td>
    * <td>@image html jittered_sampler_spp_25.png "samplesPerPixel=25"</td>
    * </tr></table>
    */
  class JitteredSampler : public Sampler {
  public:
    std::shared_ptr<SampleStream> sharedStream(int sampleIndex, uint64_t pixelHash) const override;
    SampleStream* appendStream(SampleStreamStorage& storage, int sampleIndex,
                               uint64_t pixelHash) const override;

  protected:
    std::vector<Vector2d> generateSet() override;
  };
}
