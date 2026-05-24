#pragma once

#include "render/brdf/BRDF.h"
#include "core/math/Ray.h"

namespace render {
  /**
    * @brief Bidirectional Transmittance Distribution Function —
    *        the BRDF's transmission-side counterpart.
    *
    * Where a BRDF describes how light bounces *off* a surface, a
    * BTDF describes how light bends *through* it. The interface
    * extends BRDF (so a transparent material can re-use the BRDF
    * sample shape for the transmitted lobe) and adds one
    * essential predicate:
    *
    *  - `totalInternalReflection(ray, hitPoint)` — does this ray
    *    incident on this surface satisfy the TIR condition for the
    *    current refractive-index pair? `TransparentMaterial`
    *    branches on this: TIR fires the reflected lobe instead of
    *    the transmitted one.
    *
    * The TIR check is split out of the regular BRDF interface
    * because it depends on the *ray direction* relative to the
    * surface normal, not just the (out, in) pair the parent BRDF
    * methods take.
    *
    * The only concrete subclass today is `PerfectTransmitter`
    * (perfect specular refraction). Future work could add a
    * roughness-weighted transmitter (microfacet BTDF) for frosted
    * glass; the interface is shaped to accommodate that.
    *
    * @see PerfectTransmitter — concrete subclass.
    * @see BRDF — reflection counterpart.
    */
  class BTDF : public BRDF {
  public:
    /**
      * @returns true if the configured refractive index and the
      * angle between `ray.direction()` and `hitPoint.normal()` put
      * us past the critical angle for TIR. When this returns true,
      * `TransparentMaterial::shade` skips the transmitted lobe and
      * fires a full reflection instead.
      */
    virtual bool totalInternalReflection(const Rayd& ray, const HitPoint& hitPoint) const = 0;

    int flags() const override {
      return BSDF::Specular | BSDF::Transmission;
    }
  };
}
