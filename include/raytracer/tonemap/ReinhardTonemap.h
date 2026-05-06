#pragma once

#include "raytracer/tonemap/Tonemap.h"

namespace raytracer {
  /**
    * @brief Reinhard tonemap — `c / (1 + c)` per channel.
    *
    * The classical operator from Reinhard et al. 2002, "Photographic
    * Tone Reproduction for Digital Images." Applied per-channel,
    * its output is bounded in `[0, 1)` for any non-negative HDR
    * input — the subsequent `.rgb()` clamp is a no-op.
    *
    * Properties:
    *
    *  - **Smooth highlight compression.** A pixel with luminance 1.0
    *    maps to 0.5; luminance 4.0 maps to 0.8; luminance 100 maps
    *    to 0.99. Bright but not blown-out.
    *  - **Slightly desaturated highlights.** Per-channel application
    *    means a saturated red at high intensity gets compressed
    *    differently than its (already-low) green/blue channels — the
    *    ratio shifts toward white as intensity grows. This is gentle
    *    and natural-looking; it's the same behaviour photo paper
    *    has.
    *  - **No cinematic shoulder.** The curve is a pure rational
    *    function; it doesn't have the steep midtone bias and the
    *    "filmic" look that ACES does.
    *
    * Self-registers with `TonemapFactory` as `"Reinhard"`.
    */
  class ReinhardTonemap : public Tonemap {
  public:
    inline Colord apply(const Colord& hdr) const override {
      return Colord(
        hdr.r() / (1.0 + hdr.r()),
        hdr.g() / (1.0 + hdr.g()),
        hdr.b() / (1.0 + hdr.b())
      );
    }
  };
}
