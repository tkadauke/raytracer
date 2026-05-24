#pragma once

#include <algorithm>
#include <cmath>

#include "render/tonemap/Tonemap.h"

namespace render {
  /**
    * @brief ACES filmic tonemap — Narkowicz polynomial fit to the
    *        Academy Color Encoding System reference RRT+ODT pipeline.
    *
    * The exact ACES pipeline runs many stages (Reference Rendering
    * Transform → Output Device Transform → display encoding) and
    * involves matrix multiplications between colour spaces. Krzysztof
    * Narkowicz published a tight polynomial approximation of the
    * combined RRT+ODT in 2015 ("ACES Filmic Tone Mapping Curve"):
    *
    *     f(x) = (x · (a·x + b)) / (x · (c·x + d) + e)
    *     where  a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14
    *
    * The polynomial is what every modern game engine (Unreal, Unity)
    * actually ships when they say "ACES tonemap." It's not exact —
    * the colour-space matrices are skipped — but it captures the
    * load-bearing perceptual properties:
    *
    *  - **Strong shoulder.** Highlights compress aggressively but
    *    *gradually*; specular highlights and emissive surfaces glow
    *    rather than blow out.
    *  - **Deep blacks.** The toe of the curve crushes near-zero
    *    values harder than Reinhard does, giving "cinematic"
    *    contrast.
    *  - **Slightly warm midtones.** A side-effect of the per-channel
    *    fit; the warm cast is close enough to film stock that it
    *    reads as a "look" rather than a tint.
    *
    * Output is clamped to `[0, 1]` to suppress the small overshoot
    * the polynomial fit can produce around the upper inflection.
    *
    * Self-registers with `TonemapFactory` as `"ACES"`.
    */
  class AcesTonemap : public Tonemap {
  public:
    inline Colord apply(const Colord& hdr) const override {
      return Colord(applyChannel(hdr.r()), applyChannel(hdr.g()), applyChannel(hdr.b()));
    }

  private:
    static inline double applyChannel(double x) {
      if (!std::isfinite(x)) {
        return x > 0.0 ? 1.0 : 0.0;
      }
      if (x <= 0.0) {
        return 0.0;
      }

      constexpr double a = 2.51;
      constexpr double b = 0.03;
      constexpr double c = 2.43;
      constexpr double d = 0.59;
      constexpr double e = 0.14;
      double y = (x * (a * x + b)) / (x * (c * x + d) + e);
      return std::clamp(y, 0.0, 1.0);
    }
  };
}
