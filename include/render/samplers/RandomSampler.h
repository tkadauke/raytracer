#pragma once
#include <vector>

#include "render/samplers/BuiltInSampler.h"

namespace render {
  /**
    * Samples the pixel with independent random points. This removes the
    * grid structure of `RegularSampler` / `JitteredSampler`, so clumping
    * and empty regions are possible; see `Sampler` for the interactive
    * sampler-stream widget that compares the patterns and stream
    * dimensions.
    * 
    * <table><tr>
    * <td>@image html random_sampler_spp_1.png "samplesPerPixel=1"</td>
    * <td>@image html random_sampler_spp_4.png "samplesPerPixel=4"</td>
    * <td>@image html random_sampler_spp_9.png "samplesPerPixel=9"</td>
    * <td>@image html random_sampler_spp_16.png "samplesPerPixel=16"</td>
    * <td>@image html random_sampler_spp_25.png "samplesPerPixel=25"</td>
    * </tr></table>
    */
  class RandomSampler : public BuiltInSampler {
  protected:
    std::vector<Vector2d> generateSet() override;
  };
}
