#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"

class HitPoint;

namespace render {
  /**
    * @brief Bidirectional Reflectance Distribution Function — the
    *        per-direction scattering model for a surface.
    *
    * A BRDF answers: "given an outgoing direction `out`, how much
    * light scattered in incoming direction `in` will reach the
    * camera?" Materials compose one or more BRDF lobes — Lambertian
    * for diffuse, Phong / GGX for specular highlights, perfect
    * specular for mirror reflection — and weight + sum the results.
    *
    * The class has three virtual entry points so callers can pick
    * the cheapest one for their use case:
    *
    *  - `calculate(hitPoint, out, in)` — returns the BRDF value for
    *    a fully specified light/view pair. Used by direct-lighting
    *    loops (Phong: iterate over each scene light, evaluate at
    *    that light's `in` direction).
    *  - `reflectance(hitPoint, out)` — total hemispherical
    *    reflectance integrated over `in`. Used for ambient terms
    *    where there's no specific incoming direction to evaluate.
    *  - `sample(hitPoint, out, in)` — *generates* an incoming
    *    direction (writes it into `in`) and returns the BRDF value
    *    for that draw. Used by perfect-specular materials and the
    *    importance-sampling path that's coming with the future path
    *    tracer; the caller passes the result back through
    *    `raytracer->rayColor(reflectedRay, state)`.
    *
    * The default implementations of all three return black —
    * concrete BRDFs override the methods they implement and leave
    * the others at the no-op default.
    *
    * Note the `operator()` overload: takes `(hitPoint, out, in)`
    * but forwards as `calculate(hitPoint, in, out)` (parameters
    * swapped). This is a leftover quirk of an early refactor;
    * prefer the named methods for clarity.
    *
    * @see Lambertian, PhongSpecular, PerfectSpecular — concrete
    *      BRDFs.
    * @see BTDF — the transmittance counterpart.
    */
  class BRDF {
  public:
    /// Convenience operator that forwards to `calculate`. Note the
    /// argument order is swapped: `(hitPoint, out, in)` here calls
    /// `calculate(hitPoint, in, out)` — vestigial; prefer the
    /// named methods.
    inline Colord operator()(const HitPoint& hitPoint, const Vector3d& out, const Vector3d& in) const {
      return calculate(hitPoint, in, out);
    }

    /**
      * Evaluate the BRDF at a fully specified light/view pair.
      * `out` is the direction back toward the camera; `in` is the
      * direction toward the light.
      *
      * Default returns black; override in concrete diffuse /
      * specular BRDFs.
      */
    virtual Colord calculate(const HitPoint& hitPoint, const Vector3d& out, const Vector3d& in) const;

    /**
      * Total hemispherical reflectance — the integral of `calculate`
      * over all `in`. Used for the ambient term, which has no
      * single incoming direction.
      *
      * Default returns black; override where a closed-form
      * hemispherical integral exists (e.g. Lambertian: just the
      * diffuse colour).
      */
    virtual Colord reflectance(const HitPoint& hitPoint, const Vector3d& out) const;

    /**
      * Generate an incoming direction by importance-sampling the
      * BRDF. Writes the chosen direction into `in` and returns the
      * BRDF value at that draw.
      *
      * For perfect-specular BRDFs this is deterministic
      * (mirror reflection of `out` about the normal). For
      * stochastic BRDFs (future GGX, future Lambertian-importance)
      * this draws against the lobe shape.
      *
      * Default returns black; concrete specular BRDFs override.
      */
    virtual Colord sample(const HitPoint& hitPoint, const Vector3d& out, Vector3d& in) const;
  };
}
