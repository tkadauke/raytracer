#pragma once

#include "render/tonemap/Tonemap.h"

namespace render {
  /**
    * @brief Pass-through "tonemap" — returns the HDR pixel unchanged.
    *
    * The `Colord::rgb()` quantisation that follows in
    * `Raytracer::render(Buffer<unsigned int>&)` does the actual
    * clamp-to-[0,1] and 8-bit packing. This operator does no
    * perceptual compression — it's the regression baseline when
    * comparing against the compressing operators (`Reinhard`,
    * `ACES`), and the default when no specific look is requested.
    *
    * Self-registers with `TonemapFactory` as `"Linear"`.
    */
  class LinearTonemap : public Tonemap {
  public:
    inline Colord apply(const Colord& hdr) const override {
      return hdr;
    }

    inline const char* fingerprintType() const override {
      return "LinearTonemap";
    }

    inline GpuDisplayResolveTonemap gpuDisplayResolveTonemap() const override {
      return GpuDisplayResolveTonemap::Linear;
    }
  };
}
