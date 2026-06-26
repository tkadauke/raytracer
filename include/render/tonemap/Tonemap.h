#pragma once

#include "core/Color.h"
#include "render/Object.h"

namespace render {
  enum class GpuDisplayResolveTonemap { Unsupported, Linear };

  /**
    * @brief HDR-to-LDR tone-mapping operator.
    *
    * Applied as the final step of `Raytracer::render(Buffer<unsigned int>&)`,
    * after the float framebuffer has been filled by the per-tile
    * worker threads. Each Colord pixel in the HDR buffer is passed
    * through `apply(hdr) → ldr`, then the LDR result's `.rgb()` does
    * the clamp-and-quantize to 8-bit RGB for display.
    *
    * The three operators ship today, side by side on the same scene
    * — a checker floor, three matte spheres, sky-blue background.
    * Modest dynamic range (no emissive surfaces in this codebase
    * yet) but enough to show the relative behaviour:
    *
    * <table><tr>
    * <td>@image html tonemap_linear.png "Linear (default) — pass-through, hard clamp at 1.0"</td>
    * <td>@image html tonemap_reinhard.png "Reinhard — c/(1+c), compressed everywhere"</td>
    * <td>@image html tonemap_aces.png "ACES — punchy midtones, cyan-tinted sky"</td>
    * </tr></table>
    *
    * The rendered images show the *qualitative* effect; the
    * underlying *transfer functions* — what each operator
    * actually does to a given input value — are what make the
    * rendered differences predictable. Use the input HDR luminance slider in
    * the widget below to see where each operator maps your chosen
    * input HDR luminance: at x=1.0 Linear is at its clamp ceiling,
    * Reinhard is at exactly 0.5, and ACES is at ~0.8 (the
    * "punchy midtone" claim, made concrete). At x=4.0 all three
    * have converged toward 1.0 but along very different curves.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="tonemap_curves.js"></script>
    * @endhtmlonly
    *
    * The operator works on a single pixel at a time and is stateless
    * — no per-image normalisation, no histogram pass. That makes the
    * tonemap tile-thread-safe and trivially reorderable; the cost is
    * that the brightness control comes from `Raytracer` settings and
    * the operator's shape, not from any auto-exposure on the rendered
    * image. (Auto-exposure / histogram-based operators would need a
    * pre-pass over the full HDR buffer; that's a future addition.)
    *
    * Concrete operators ship in this directory:
    *
    *  - `LinearTonemap` — pass-through. The HDR pixel goes straight
    *    to `Colord::rgb()`, which clamps to [0, 1] and quantizes —
    *    no perceptual compression. Default.
    *  - `ReinhardTonemap` — `c / (1 + c)`, applied per-channel. The
    *    classical operator: simple, gentle compression of highlights,
    *    natural-looking results without any cinematic baking.
    *  - `AcesTonemap` — Narkowicz's polynomial fit to the ACES
    *    filmic curve. Strong shoulder, deep blacks, *punchy*
    *    midtones (Reinhard maps luminance 1 to 0.5; ACES maps it
    *    to ~0.8). Closer to what film cameras and modern game
    *    engines produce.
    *
    * @see Raytracer::setTonemap — how to install one.
    */
  class Tonemap : public render::Object {
  public:
    virtual ~Tonemap() = default;

    /**
      * Map an HDR Colord (any non-negative range) to an LDR Colord
      * suitable for `.rgb()` quantisation. Returning the input
      * unchanged is valid — that's what `LinearTonemap` does and it
      * matches the pre-tonemap behaviour. Operators that compress
      * highlights (`Reinhard`, `ACES`) keep the result in `[0, 1]`
      * by construction so the final `.rgb()` clamp is a no-op.
      */
    virtual Colord apply(const Colord& hdr) const = 0;

    /// Stable type name for deterministic fingerprints and diagnostics.
    virtual const char* fingerprintType() const = 0;

    /**
      * Return the platform display-resolve implementation this tonemap can use.
      *
      * The default is unsupported because a GPU path-loop backend must not
      * silently bypass a CPU tonemap with different transfer behavior.
      */
    virtual GpuDisplayResolveTonemap gpuDisplayResolveTonemap() const {
      return GpuDisplayResolveTonemap::Unsupported;
    }
  };
}
