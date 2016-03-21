#pragma once

#include "raytracer/samplers/Sampler.h"

namespace raytracer {
  /**
    * Samples the pixels in a regular grid, but each ray is displaced by a
    * random amount within its own cell.
    * 
    * <table><tr>
    * <td>@image html random_sampler_spp_1.png "samplesPerPixel=1"</td>
    * <td>@image html random_sampler_spp_4.png "samplesPerPixel=4"</td>
    * <td>@image html random_sampler_spp_9.png "samplesPerPixel=9"</td>
    * <td>@image html random_sampler_spp_16.png "samplesPerPixel=16"</td>
    * <td>@image html random_sampler_spp_25.png "samplesPerPixel=25"</td>
    * </tr></table>
    */
  class RandomSampler : public Sampler {
  protected:
    virtual std::vector<Vector2d> generateSet();
  };
}
