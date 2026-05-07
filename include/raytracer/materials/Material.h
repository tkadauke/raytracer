#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"
#include "core/math/Ray.h"

#include "render/Object.h"

class HitPoint;

namespace raytracer {
  class Raytracer;
  class State;

  /**
    * @brief Abstract base for everything that can be assigned to a
    *        primitive and shaded.
    *
    * The renderer's contract is one method: `shade` takes the
    * primary ray, the hit point along it, and a mutable `State`,
    * and returns a colour. Subclasses are responsible for
    * synthesising the BRDF / BTDF lobes, calling `Raytracer::rayColor`
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
    * `Raytracer` increments `state.recursionDepth` on every
    * `rayColor` call, so a well-formed `shade` either returns a
    * direct-lit colour or delegates further work back through
    * `raytracer->rayColor(...)`.
    *
    * @see PhongMaterial — the canonical worked example.
    * @see BRDF / BTDF — the reflectance / transmittance lobes
    *      composed by these materials.
    */
  class Material : public render::Object {
  public:
    virtual ~Material() {}

    /**
      * Shade `hitPoint` along `ray`. Returns the colour produced by
      * this material — direct lighting, recursive reflection,
      * refraction, and any combination thereof.
      *
      * Implementations should:
      *
      *  - Read `raytracer->scene()->lights()` and `ambient()` for
      *    direct lighting.
      *  - Use `raytracer->rayColor(reflected, state)` for any
      *    recursive components.
      *  - Bump shadow-ray counters on `state` via the appropriate
      *    `state.shadowHit`/`shadowMiss` calls.
      *
      * `state.events` (when populated) is the right place to record
      * material-level branch decisions — `TransparentMaterial`
      * emits "TIR" / "Tracing reflection" / "Tracing transmission"
      * events here.
      */
    virtual Colord shade(const Raytracer* raytracer, const Rayd& ray, const HitPoint& hitPoint, State& state) const = 0;
  };
}
