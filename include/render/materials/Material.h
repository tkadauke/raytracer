#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"
#include "core/math/Ray.h"

#include "render/Object.h"

class HitPoint;

namespace render {
  class RayCaster;
  class Scene;
  class State;

  /**
    * Result of `Material::sampleBsdf`: the importance-sampled outgoing
    * direction, the BSDF value (already including any cosine term the
    * material wants to fold in), the density that the sample was drawn
    * with, and whether the sample is a delta event (the integrator
    * must NOT divide by `pdf` for delta samples; the value returned is
    * already the post-cancellation finite contribution).
    */
  struct MaterialBsdfSample {
    Vector3d direction{Vector3d::null};
    Colord value{Colord::black()};
    double pdf{0.0};
    bool isDelta{false};
  };
}

namespace render {

  /**
    * @brief Abstract base for everything that can be assigned to a
    *        primitive and shaded.
    *
    * The renderer's contract is one method: `shade` takes the
    * primary ray, the hit point along it, and a mutable `State`,
    * and returns a colour. Subclasses are responsible for
    * synthesising the BRDF / BTDF lobes, calling `RayCaster::rayColor`
    * recursively for reflections / refractions, and reading the
    * `Scene::lights` for direct lighting.
    *
    * Concrete materials in this codebase:
    *
    *  - `MatteMaterial` — render::Lambertian diffuse only.
    *  - `PhongMaterial` — render::Lambertian + Phong specular highlight.
    *  - `ReflectiveMaterial` — `PhongMaterial` + perfect mirror
    *    reflection.
    *  - `TransparentMaterial` — `PhongMaterial` + perfect specular
    *    + perfect refraction (with TIR fallback).
    *
    * `shade` may not call other methods on the same material in a
    * way that would re-enter the recursion limit unguarded — the
    * active recursive `RayCaster` callback updates the `State` for
    * each `rayColor` call, so a well-formed `shade` either returns a
    * direct-lit colour or delegates further work back through
    * `raycaster->rayColor(...)`.
    *
    * @see PhongMaterial — the canonical worked example.
    * @see BRDF / BTDF — the reflectance / transmittance lobes
    *      composed by these materials.
    */
  class Material : public render::Object {
  public:
    enum class Sidedness { Front, Back, TwoSided };
    enum class RasterRecursiveFallback { None, ReflectiveLocalPhong, TransparentAlphaPhong };

    virtual ~Material() {
    }

    inline Sidedness sidedness() const {
      return m_sidedness;
    }

    inline void setSidedness(Sidedness sidedness) {
      m_sidedness = sidedness;
    }

    /**
      * Shade `hitPoint` along `ray`. Returns the colour produced by
      * this material — direct lighting, recursive reflection,
      * refraction, and any combination thereof.
      *
      * Implementations should:
      *
      *  - Read `scene.lights()` and `ambient()` for direct lighting.
      *  - Use `raycaster->rayColor(reflected, state)` for any
      *    recursive components.
      *  - Bump shadow-ray counters on `state` via the appropriate
      *    `state.shadowHit`/`shadowMiss` calls.
      *
      * `state.events` (when populated) is the right place to record
      * material-level branch decisions — `TransparentMaterial`
      * emits "TIR" / "Tracing reflection" / "Tracing transmission"
      * events here.
      */
    virtual Colord shade(const render::RayCaster* raycaster, const render::Scene& scene,
                         const Rayd& ray, const HitPoint& hitPoint, render::State& state) const = 0;

    /**
      * Reports whether this material can be sampled directly by a
      * path-tracing integrator. Defaults to `false`; the path tracer
      * falls back to `shade()` (Whitted behavior, no further bounces)
      * for materials that haven't been refactored yet.
      *
      * Materials returning `true` must implement `evalBsdf`,
      * `sampleBsdf`, and `bsdfPdf`. Returning `true` from a material
      * whose `shade()` does its own recursion (e.g. ReflectiveMaterial,
      * TransparentMaterial) is incorrect — the path tracer owns
      * recursion through `sampleBsdf`.
      */
    virtual bool supportsBsdfSampling() const {
      return false;
    }

    /**
      * Evaluate the BSDF for the given incoming/outgoing direction
      * pair. Both vectors point AWAY from the surface; `wi` is the
      * direction the integrator is gathering from (back along the
      * incoming ray), `wo` is the direction radiance is leaving in.
      * Returns black for materials without BSDF support or for delta
      * lobes (the value isn't finite there). Default returns black.
      */
    virtual Colord evalBsdf(const HitPoint& /*hitPoint*/, const Vector3d& /*wi*/,
                            const Vector3d& /*wo*/) const {
      return Colord::black();
    }

    /**
      * Importance-sample an outgoing direction using a caller-owned
      * 2D random sample in `[0, 1]²`. Returns a `MaterialBsdfSample`
      * with direction/value/pdf/isDelta; integrators use `value / pdf`
      * (or just `value` for delta samples) to weight the recursive
      * contribution. Default returns zero-pdf to signal an
      * unsampleable material.
      */
    virtual MaterialBsdfSample sampleBsdf(const HitPoint& /*hitPoint*/, const Vector3d& /*wi*/,
                                          const Vector2d& /*sample*/) const {
      return MaterialBsdfSample();
    }

    /**
      * Probability density that `sampleBsdf(wi)` would have produced
      * `wo`. Used by MIS weight calculations. Returns 0 for delta
      * lobes or for materials without BSDF support. Default returns 0.
      */
    virtual double bsdfPdf(const HitPoint& /*hitPoint*/, const Vector3d& /*wi*/,
                           const Vector3d& /*wo*/) const {
      return 0.0;
    }

    virtual RasterRecursiveFallback rasterRecursiveFallback() const {
      return RasterRecursiveFallback::None;
    }

    virtual double rasterPreviewAlpha() const {
      return 1.0;
    }

    virtual const char* rasterRecursiveFallbackWarning() const {
      return nullptr;
    }

  private:
    Sidedness m_sidedness{Sidedness::TwoSided};
  };
}
