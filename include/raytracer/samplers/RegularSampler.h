#pragma once

#include "raytracer/samplers/Sampler.h"

namespace raytracer {
  /**
    * Samples the pixels in a regular grid.
    * 
    * <table><tr>
    * <td>@image html regular_sampler_spp_1.png "samplesPerPixel=1"</td>
    * <td>@image html regular_sampler_spp_4.png "samplesPerPixel=4"</td>
    * <td>@image html regular_sampler_spp_9.png "samplesPerPixel=9"</td>
    * <td>@image html regular_sampler_spp_16.png "samplesPerPixel=16"</td>
    * <td>@image html regular_sampler_spp_25.png "samplesPerPixel=25"</td>
    * </tr></table>
    */
  class RegularSampler : public Sampler {
  protected:
    virtual std::vector<Vector2d> generateSet();
  };
}
