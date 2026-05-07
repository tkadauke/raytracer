#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"

class HitPoint;

namespace render {
  /**
    * @brief Bidirectional Scattering Distribution Function — the
    *        unifying interface over reflection and transmission
    *        scattering models.
    *
    * A `BSDF` answers three questions an integrator wants asked at
    * every hit point:
    *
    *  - `eval(wi, wo)` — for a fully specified direction pair, what
    *    fraction of the radiance leaving along `wo` came from the
    *    radiance arriving along `wi`? Returns black for delta lobes
    *    (perfect mirror, perfect refraction) — the value is a Dirac
    *    there and isn't expressible as a finite spectrum. Used by
    *    direct-lighting loops (Whitted: iterate scene lights,
    *    evaluate per-light) and by multiple-importance-sampling
    *    integrators that need the BSDF value at a light-sampled
    *    direction.
    *  - `sample(wi, wo, pdf)` — generate an `wo` by importance-
    *    sampling the lobe, write the chosen direction into `wo`
    *    and the pdf of having drawn it into `pdf`, return the BSDF
    *    value at that draw. Used by recursive integrators to spawn
    *    the next ray. Specular lobes report `pdf == 1` to mark the
    *    delta case — the integrator must NOT divide by the pdf for
    *    such draws (the delta and the implicit Dirac in the
    *    rendering equation cancel, and the value returned is
    *    already the post-cancellation finite result).
    *  - `pdf(wi, wo)` — the probability density that `sample(wi)`
    *    would have produced this `wo`. Used by MIS weight
    *    calculations. Returns 0 for delta lobes.
    *
    * Plus one auxiliary entry point retained from the BRDF history:
    *
    *  - `reflectance(wi)` — total hemispherical reflectance over
    *    `wo`, used by the ambient term where there's no specific
    *    incoming direction to evaluate against.
    *
    * Direction convention: both `wi` and `wo` are unit vectors
    * pointing AWAY from the surface. `wi` is the "input" — the
    * direction back toward the camera (or, in a recursive bounce,
    * back toward the previous surface). `wo` is the "output" — the
    * direction we're sampling, or the direction toward the light
    * during direct lighting. The naming follows the input/output
    * model of the integrator, not the radiometric convention where
    * `wi` would be the incoming light direction.
    *
    * `flags()` classifies the lobe along two independent axes:
    * Diffuse / Glossy / Specular and Reflection / Transmission. The
    * integrator uses these to skip MIS for specular lobes, to tag
    * the wavelength for spectral path tracing, etc.
    *
    * Concrete BSDFs in this codebase are the existing BRDF /
    * BTDF subclasses — `Lambertian`, `GlossySpecular`,
    * `PerfectSpecular`, `PerfectTransmitter` — which inherit from
    * `BSDF` via `BRDF` and provide the lobe-specific overrides. New
    * BSDFs (Cook-Torrance, GGX-VNDF, Disney Principled, …) will
    * subclass `BSDF` directly.
    *
    * @see BRDF — reflection-only base; inherits BSDF and forwards
    *      eval/sample/pdf to its `calculate`/`sample` shape.
    * @see BTDF — transmission-only base; adds the TIR predicate.
    */
  class BSDF {
  public:
    enum Flags {
      None         = 0,
      Diffuse      = 1 << 0,
      Glossy       = 1 << 1,
      Specular     = 1 << 2,
      Reflection   = 1 << 3,
      Transmission = 1 << 4,
    };

    virtual ~BSDF() = default;

    /// Evaluate `f(wi, wo)`. Returns black for delta (specular)
    /// lobes — the value isn't finite there.
    virtual Colord eval(const HitPoint& hitPoint, const Vector3d& wi, const Vector3d& wo) const = 0;

    /// Importance-sample an outgoing direction. Writes the chosen
    /// `wo` and the pdf of drawing it into the out-params; returns
    /// the BSDF value at that draw. Delta lobes set `pdf = 1`.
    virtual Colord sample(const HitPoint& hitPoint, const Vector3d& wi, Vector3d& wo, double& pdf) const = 0;

    /// Density that `sample(wi)` would have produced `wo`. 0 for
    /// delta lobes.
    virtual double pdf(const HitPoint& hitPoint, const Vector3d& wi, const Vector3d& wo) const = 0;

    /// Total hemispherical reflectance — the integral of `eval`
    /// over `wo`. Used by the Whitted integrator's ambient term.
    /// Default returns black; override where a closed-form integral
    /// exists (e.g. Lambertian: just the diffuse colour).
    virtual Colord reflectance(const HitPoint& hitPoint, const Vector3d& wi) const;

    /// Bitfield of `Flags` classifying the lobe. Default `None`.
    virtual int flags() const { return None; }

    inline bool isSpecular() const { return (flags() & Specular) != 0; }
    inline bool isDiffuse() const { return (flags() & Diffuse) != 0; }
    inline bool isGlossy() const { return (flags() & Glossy) != 0; }
    inline bool isReflection() const { return (flags() & Reflection) != 0; }
    inline bool isTransmission() const { return (flags() & Transmission) != 0; }
  };
}
